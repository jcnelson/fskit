/*
   fskit: a library for creating multi-threaded in-RAM filesystems
   Copyright (C) 2014  Jude Nelson

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "readdir.h"
#include "route.h"
#include "util.h"

// initialize a directory entry from an fskit_entry
// return the new entry on success
// return NULL if out-of-memory
static struct fskit_dir_entry* fskit_make_dir_entry( struct fskit_entry* dent, char const* name ) {

   struct fskit_dir_entry* dir_ent = CALLOC_LIST( struct fskit_dir_entry, 1 );
   if( dir_ent == NULL ) {
      // out of memory 
      return NULL;
   }
   
   dir_ent->type = dent->type;
   dir_ent->file_id = dent->file_id;
   dir_ent->name = strdup( name );
   
   if( dir_ent->name == NULL ) {
      // out of memory 
      safe_free( dir_ent );
      return NULL;
   }
   
   return dir_ent;
}


// free a dir entry 
void fskit_dir_entry_free( struct fskit_dir_entry* dir_ent ) {
   
   if( dir_ent == NULL ) {
      return;
   }
   
   if( dir_ent->name != NULL ) {
      safe_free( dir_ent->name );
      dir_ent->name = NULL;
   }
   
   safe_free( dir_ent );
}

// free a list of dir entries 
void fskit_dir_entry_free_list( struct fskit_dir_entry** dir_ents ) {
   
   for( uint64_t i = 0; dir_ents[i] != NULL; i++ ) {
      
      if( dir_ents[i] != NULL ) {
         fskit_dir_entry_free( dir_ents[i] );
      }
   }
   
   safe_free( dir_ents );
}


// run the user-supplied route for listdir 
// return 1 if the given dent should be included in the listing 
// return 0 if the given dent should NOT be included in the listing 
// return negative on error
int fskit_run_user_readdir( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_dir_entry** dents, uint64_t num_dents ) {
   
   int rc = 0;
   int cbrc = 0;
   struct fskit_route_dispatch_args dargs;
   
   fskit_route_readdir_args( &dargs, dents, num_dents );
   
   rc = fskit_route_call_readdir( core, path, fent, &dargs, &cbrc );
   
   if( rc == -EPERM || rc == -ENOSYS ) {
      // no routes 
      return 0;
   }
   
   return cbrc;
}


// low-level read directory--read up to num_children directory entires from dent, starting with the child at child_offset
// dent must be a directory.
// dent must be read-locked.
// On success, returns a duplicated copy of a range of the given directory's children (starting at the given offset), serialized as fskit_dir_entry structures.  It will be terminated with a NULL pointer.
// Also, it sets *num_read to the number of dir entries in the returned range.
// On error, returns NULL and sets *err to:
// * -EDEADLK if there was a deadlock (this is a bug, and should be reported)
// * -ENOMEM if there was insuffucient memory
// If there are no children left to read (i.e. child_offset is beyond the end of the directory), then this method sets *err to 0 and returns NULL to indicate EOD
static struct fskit_dir_entry** fskit_readdir_lowlevel( struct fskit_core* core, char const* fs_path, struct fskit_entry* dent, uint64_t child_offset, uint64_t num_children, uint64_t* num_read, int* err ) {
   
   int rc = 0;
   uint64_t next = 0;
   
   // sanity check 
   if( dent->children->size() <= child_offset ) {
      
      // reading off the end of the directory 
      // return EOF condition 
      *err = 0;
      *num_read = 0;
      return NULL;
   }
   
   // will read at least one child 
   uint64_t max_read = MIN( dent->children->size() - child_offset, num_children );
   
   // allocate the children 
   struct fskit_dir_entry** dir_ents = CALLOC_LIST( struct fskit_dir_entry*, max_read + 1 );
   if( dir_ents == NULL ) {
      // out of memory
      *err = -ENOMEM;
      return NULL;  
   }
   
   for( uint64_t i = child_offset; i < dent->children->size() && next < max_read; i++ ) {
      
      // extract values from iterators
      struct fskit_entry* fent = fskit_entry_set_child_at( dent->children, i );
      long fent_name_hash = fskit_entry_set_name_hash_at( dent->children, i );

      // skip NULL children 
      if( fent == NULL ) {
         continue;
      }
      
      // next directory entry
      struct fskit_dir_entry* dir_ent = NULL;

      // handle . and .. separately--we only want to lock children (not the current or parent directory)
      if( fent_name_hash == fskit_entry_name_hash( "." ) ) {
         
         // handle .
         dir_ent = fskit_make_dir_entry( dent, "." );
      }
      else if( fent_name_hash == fskit_entry_name_hash( ".." ) ) {
         
         // handle ..
         // careful--the directory .. can be the same as dent if we're dealing with root.
         if( dent != fent ) {
            
            rc = fskit_entry_rlock( fent );
            if( rc != 0 ) {
               
               // shouldn't happen--indicates deadlock 
               fskit_error("fskit_entry_rlock(%p) rc = %d\n", fent, rc );
               fskit_dir_entry_free_list( dir_ents );
               
               *err = rc;
               return NULL;
            }
         }
              
         dir_ent = fskit_make_dir_entry( fent, ".." );
         
         if( dent != fent ) {
            fskit_entry_unlock( fent );
         }
      }
      else {
         
         // handle normal entry 
         rc = fskit_entry_rlock( fent );
         if( rc != 0 ) {
            
            // shouldn't happen--indicates deadlock
            fskit_error("BUG: fskit_entry_rlock(%p) rc = %d\n", fent, rc );
            fskit_dir_entry_free_list( dir_ents );
            
            *err = rc;
            return NULL;
         }
         
         // skip over entries that are being deleted 
         if( fent->deletion_in_progress || fent->type == FSKIT_ENTRY_TYPE_DEAD || fent->name == NULL ) {
            continue;
         }
         
         // snapshot this entry
         dir_ent = fskit_make_dir_entry( fent, fent->name );
         
         fskit_entry_unlock( fent );
      }

      // do we have an entry?
      if( dir_ent != NULL ) {
         
         dir_ents[next] = dir_ent;
         next++;
      }
      else {
         
         // out of memory
         *err = -ENOMEM;
         break;
      }
   }
   
   *num_read = next;
   
   return dir_ents;
}


// user-called method to omit directories from a directory listing 
int fskit_readdir_omit( struct fskit_dir_entry** dents, int i ) {
   
   fskit_dir_entry_free( dents[i] );
   dents[i] = NULL;
   
   return 0;
}


// compactify a list of directory entries, so there are no NULLs in the list.
// return 0 on success
// return -ENOMEM on allocation failure
int fskit_readdir_compactify_list( struct fskit_dir_entry*** ret_entries, uint64_t num_entries, uint64_t* ret_new_size ) {
   
   uint64_t new_size = num_entries;
   struct fskit_dir_entry** entries = *ret_entries;
   
   for( uint64_t i = 0; i < num_entries; i++ ) {
      
      if( entries[i] == NULL ) {
         new_size--;
      }
   }
   
   if( new_size == num_entries ) {
      // no compactification needed 
      *ret_new_size = num_entries;
      return 0;
   }
   
   struct fskit_dir_entry** new_entries = CALLOC_LIST( struct fskit_dir_entry*, new_size + 1 );
   
   if( new_entries == NULL ) {
      return -ENOMEM;
   }
   
   uint64_t j = 0;
   for( uint64_t i = 0; i < num_entries; i++ ) {
      
      if( entries[i] != NULL ) {
         new_entries[j] = entries[i];
         j++;
      }
   }
   
   *ret_entries = new_entries;
   *ret_new_size = new_size;
   free( entries );
   
   return 0;
}


// read data from a directory, using the given directory handle.
// starts reading at child_offset, and reads at most a range of num_children (the actual value will be set into *num_read)
// on failure, it sets *err to one of the following:
// * -ENOMEM if no memory 
// * -EDEADLK if there would be deadlock (this is a bug if it happens)
// * -EBADF if the directory hadndle is invalid 
struct fskit_dir_entry** fskit_readdir( struct fskit_core* core, struct fskit_dir_handle* dirh, uint64_t child_offset, uint64_t num_children, uint64_t* num_read, int* err ) {
   
   int rc = 0;
   
   rc = fskit_dir_handle_rlock( dirh );
   if( rc != 0 ) {
      // shouldn't happen--indicates deadlock 
      fskit_error("fskit_dir_handle_rlock(%p) rc = %d\n", dirh, rc );
      *err = rc;
      return NULL;
   }
   
   // sanity check
   if( dirh->dent == NULL ) {
      
      // invalid
      fskit_dir_handle_unlock( dirh );
      *err = -EBADF;
      return NULL;
   }

   rc = fskit_entry_rlock( dirh->dent );
   if( rc != 0 ) {
      // shouldn't happen--indicates deadlock 
      fskit_error("fskit_entry_rlock(%p) rc = %d\n", dirh->dent, rc );
      *err = rc;
      
      fskit_dir_handle_unlock( dirh );
      return NULL;
   }

   struct fskit_dir_entry** dents = fskit_readdir_lowlevel( core, dirh->path, dirh->dent, child_offset, num_children, num_read, err );
   
   if( dents != NULL ) {
      rc = fskit_run_user_readdir( core, dirh->path, dirh->dent, dents, *num_read );
      if( rc != 0 ) {
         
         fskit_dir_entry_free_list( dents );
         *num_read = 0;
         *err = rc;
      }
      
      // compactify results 
      uint64_t new_num_read = 0;
      rc = fskit_readdir_compactify_list( &dents, *num_read, &new_num_read );
      
      if( rc != 0 ) {
      
         fskit_dir_entry_free_list( dents );
         *num_read = 0;
         *err = rc;
      }
      else {
         *num_read = new_num_read;
      }
   }
   
   fskit_entry_unlock( dirh->dent );
   
   fskit_dir_handle_unlock( dirh );
   
   return dents;
}

// list a whole directory's data
struct fskit_dir_entry** fskit_listdir( struct fskit_core* core, struct fskit_dir_handle* dirh, uint64_t* num_read, int* err ) {
   return fskit_readdir( core, dirh, 0, UINT64_MAX, num_read, err );
}
