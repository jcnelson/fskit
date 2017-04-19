/*
   fskit: a library for creating multi-threaded in-RAM filesystems
   Copyright (C) 2014  Jude Nelson

   This program is dual-licensed: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License version 3 or later as
   published by the Free Software Foundation. For the terms of this
   license, see LICENSE.LGPLv3+ or <http://www.gnu.org/licenses/>.

   You are free to use this program under the terms of the GNU Lesser General
   Public License, but WITHOUT ANY WARRANTY; without even the implied
   warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   See the GNU Lesser General Public License for more details.

   Alternatively, you are free to use this program under the terms of the
   Internet Software Consortium License, but WITHOUT ANY WARRANTY; without
   even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   For the terms of this license, see LICENSE.ISC or
   <http://www.isc.org/downloads/software-support-policy/isc-license/>.
*/

#include <fskit/entry.h>
#include <fskit/readdir.h>
#include <fskit/route.h>
#include <fskit/util.h>

#include "fskit_private/private.h"


// table entry for seekdir/telldir positions
struct fskit_telldir_entry {
    
    char name[ FSKIT_FILESYSTEM_NAMEMAX+1 ];
    off_t offset;
    
    struct fskit_telldir_entry* next;
};

#define FSKIT_TELLDIR_ENTRY_CMP( t1, t2 ) (strcmp((t1)->name, (t2)->name))

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
   memset( dir_ent->name, 0, FSKIT_FILESYSTEM_NAMEMAX+1 );
   strncpy( dir_ent->name, name, FSKIT_FILESYSTEM_NAMEMAX );
   
   return dir_ent;
}


// free a dir entry
void fskit_dir_entry_free( struct fskit_dir_entry* dir_ent ) {

   if( dir_ent == NULL ) {
      return;
   }

   fskit_safe_free( dir_ent );
}

// free a list of dir entries
void fskit_dir_entry_free_list( struct fskit_dir_entry** dir_ents ) {

   for( uint64_t i = 0; dir_ents[i] != NULL; i++ ) {

      if( dir_ents[i] != NULL ) {
         fskit_dir_entry_free( dir_ents[i] );
      }
   }

   fskit_safe_free( dir_ents );
}


// run the user-supplied route for listdir
// return 1 if the given dent should be included in the listing
// return 0 if the given dent should NOT be included in the listing
// return negative on error
int fskit_run_user_readdir( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_dir_entry** dents, uint64_t num_dents ) {

   int rc = 0;
   int cbrc = 0;
   struct fskit_route_dispatch_args dargs;
   char name[FSKIT_FILESYSTEM_NAMEMAX+1];
   
   memset( name, 0, FSKIT_FILESYSTEM_NAMEMAX+1 );
   fskit_basename( path, name );

   fskit_route_readdir_args( &dargs, name, dents, num_dents );

   rc = fskit_route_call_readdir( core, path, fent, &dargs, &cbrc );

   if( rc == -EPERM || rc == -ENOSYS ) {
      // no routes
      return 0;
   }

   return cbrc;
}


// find the starting point where we can begin to read a directory, based on the last-read name
// set *read_itr and *read_start
static int fskit_readdir_find_start( struct fskit_dir_handle* dirh, fskit_entry_set_itr* read_itr, fskit_entry_set** read_start ) {

    // find out where we can start reading
    struct fskit_entry* dent = dirh->dent;
    
    fskit_entry_set* member = fskit_entry_set_find_itr( dent->children, dirh->curr_name );
    if( member == NULL ) {
        
        // name is invalid--try to seek to the point where we left off 
        fskit_entry_set* sought = NULL;
        bool found = false;
        
        for( sought = fskit_entry_set_begin( read_itr, dent->children ); sought != NULL; sought = fskit_entry_set_next( read_itr ) ) {
            
            // if we reached the point where the names are lexicographically greater than where we left off last, then 
            // that's the new read point 
            char const* sought_name = fskit_entry_set_name_at( sought );
            if( strcmp(dirh->curr_name, sought_name) < 0 ) {
                
                found = true;
                break;
            }
        }
        if( found ) {
            
            // scanned ahead!
            *read_start = sought;
        }
        else {
           
            // end of directory!
            *read_start = NULL;
        }
    }
    else {
        
        *read_start = member;

        // TODO: find out why sglib's init_on_equal method doesn't get us the right iterator
        fskit_entry_set* tmp = NULL;
        for( tmp = fskit_entry_set_begin( read_itr, dent->children ); tmp != NULL && tmp != member; tmp = fskit_entry_set_next( read_itr ) );
    }
    
    return 0;
}


// iterate through dent->children and return a null-terminated list of fskit_dir_entry* pointers
// On error, return NULL and:
//    set *err to ENOMEM on OOM
static struct fskit_dir_entry** fskit_readdir_itr( struct fskit_core* core, struct fskit_entry* dent, uint64_t num_children, uint64_t* num_read, fskit_entry_set* read_start, fskit_entry_set_itr* read_itr, int* err ) {
    
   int rc = 0;
   uint64_t read_count = 0;
   fskit_entry_set* entry = NULL;

   // allocate the children
   struct fskit_dir_entry** dir_ents = CALLOC_LIST( struct fskit_dir_entry*, num_children + 1 );
   if( dir_ents == NULL ) {
      // out of memory
      *err = -ENOMEM;
      return NULL;
   }

   for( entry = read_start; entry != NULL && read_count < num_children; entry = fskit_entry_set_next( read_itr ) ) {

      // extract values from iterators
      struct fskit_entry* fent = fskit_entry_set_child_at( entry );
      char const* fskit_name = fskit_entry_set_name_at( entry );

      // skip NULL children
      if( fent == NULL ) {
         continue;
      }
      
      // skip garbage-collectables
      if( fent->deletion_in_progress || fent->type == FSKIT_ENTRY_TYPE_DEAD ) {
         
         continue;
      }

      // next directory entry
      struct fskit_dir_entry* dir_ent = NULL;

      // handle . and .. separately--we only want to lock children (not the current or parent directory)
      if( strcmp( fskit_name, "." ) == 0 ) { 

         // handle .
         dir_ent = fskit_make_dir_entry( dent, "." );
      }
      else if( strcmp( fskit_name, ".." ) == 0 ) {

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
         
         if( dir_ent == NULL ) {
             
             // OOM--bail with what we have
             *err = -ENOMEM;
             break;
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
         
         // snapshot this entry
         dir_ent = fskit_make_dir_entry( fent, fskit_name );

         fskit_entry_unlock( fent );
         
         if( dir_ent == NULL ) {
             
             // OOM--bail with what we have
             *err = -ENOMEM;
             break;
         }
      }

      // do we have an entry?
      if( dir_ent != NULL ) {

         dir_ents[read_count] = dir_ent;
         read_count++;
      }
      else {

         // out of memory
         *err = -ENOMEM;
         break;
      }
   }
   
   if( *err != 0 ) {
       
       fskit_dir_entry_free_list( dir_ents );
       *num_read = 0;
       return NULL;
   }

   *num_read = read_count;   
   return dir_ents;
}


// read all of the children of a locked directory entry
// dent must be at least read-locked or write-locked
// Return null-terminated list of fskit_dir_entry* items on success
// Return NULL on error, and set *err to:
//   * -ENOMEM: oom
//   * -EINVAL: not a dir
struct fskit_dir_entry** fskit_listdir_locked( struct fskit_core* core, struct fskit_entry* dent, uint64_t* num_read, int* err ) {

   if( dent->type != FSKIT_ENTRY_TYPE_DIR ) {
      *err = -EINVAL;
      return NULL;
   }

   uint64_t num_children = fskit_entry_set_count( dent->children );
   fskit_entry_set_itr read_itr;
   fskit_entry_set* read_start = fskit_entry_set_begin( &read_itr, dent->children );

   return fskit_readdir_itr(core, dent, num_children, num_read, read_start, &read_itr, err);
}


// low-level read directory--read up to num_children directory entires from dirh->dent, starting with the last child previously read from dirh.
// dirh->dent must be a directory.
// dirh->dent must be at least read-locked.
// dirh must be write-locked
// On success, returns a duplicated copy of a range of the given directory's children (starting at the given offset), serialized as fskit_dir_entry structures.  It will be terminated with a NULL pointer.
// Also, it sets *num_read to the number of dir entries in the returned range.
// On error, returns NULL and sets *err to:
// * -EDEADLK if there was a deadlock (this is a bug, and should be reported)
// * -ENOMEM if there was insuffucient memory
// If there are no children left to read (i.e. child_offset is beyond the end of the directory), then this method sets *err to 0 and returns NULL to indicate EOD
static struct fskit_dir_entry** fskit_readdir_lowlevel( struct fskit_core* core, struct fskit_dir_handle* dirh, uint64_t num_children, uint64_t* num_read, int* err ) {

   int rc = 0;
   struct fskit_entry* dent = dirh->dent;
   
   fskit_entry_set* read_start = NULL;
   
   fskit_entry_set_itr read_itr;
   fskit_entry_set* entry = NULL;
   
   if( dirh->eof ) {
       // EOF
       *num_read = 0;
       return NULL;
   }

   if( strlen(dirh->curr_name) == 0 ) {
       
       // haven't begun reading yet 
       read_start = fskit_entry_set_begin( &read_itr, dent->children );
       strncpy( dirh->curr_name, fskit_entry_set_name_at( read_start ), FSKIT_FILESYSTEM_NAMEMAX );
   }
   else {

       fskit_entry_set_itr test_itr;
       fskit_entry_set* test_entry;

       fskit_readdir_find_start( dirh, &read_itr, &read_start );
       if( read_start == NULL ) {
           
           // out of directory 
           *num_read = 0;
           return NULL;
       }
       
       // advance to the next entry, since that's the first unread one 
       read_start = fskit_entry_set_next( &read_itr );
       if( read_start == NULL ) {
           
           // out of directory 
           *num_read = 0;
           return NULL;
       }
   }
   
   // UINT64_MAX means 'all children'
   if( num_children == UINT64_MAX ) {
      num_children = fskit_entry_set_count( dirh->dent->children );
   }

   struct fskit_dir_entry** dir_ents = fskit_readdir_itr(core, dent, num_children, num_read, read_start, &read_itr, err );
   if( dir_ents == NULL ) {
      return NULL;
   }
      
   if( *num_read == 0 ) {
       
       fskit_dir_entry_free_list( dir_ents );
       dir_ents = NULL;
       
       // further attempts to read will EOF
       dirh->eof = true;
   }
   else {
       
       // remember the last name, so we can resume there
       memset( dirh->curr_name, 0, FSKIT_FILESYSTEM_NAMEMAX+1 );
       strncpy( dirh->curr_name, dir_ents[*num_read-1]->name, FSKIT_FILESYSTEM_NAMEMAX );
   }
   
   return dir_ents;
}


// seekdir(3)--revert to a point in the directory stream where we were reading from in the past
void fskit_seekdir( struct fskit_dir_handle* dirh, off_t loc ) {
    
    fskit_dir_handle_wlock( dirh );
    
    // where did we leave off?
    for( struct fskit_telldir_entry* tent = dirh->telldir_list; tent != NULL; tent = tent->next ) {
        
        if( tent->offset == loc ) {
            
            fskit_entry_set_itr read_itr;
            fskit_entry_set* read_start = NULL;
            
            fskit_readdir_find_start( dirh, &read_itr, &read_start );
            
            if( read_start != NULL ) {
                
                // advance directory pointer 
                strncpy( dirh->curr_name, fskit_entry_set_name_at( read_start ), FSKIT_FILESYSTEM_NAMEMAX );
            }
        }
    }
    fskit_dir_handle_unlock( dirh );
}


// telldir(3)--store the current point in the directory stream where we are reading currently, so we can jump to it later.
off_t fskit_telldir( struct fskit_dir_handle* dirh ) {
    
    // random place...we just need it to be unique (w.h.p)
    off_t offset = ((uint64_t)fskit_random32() << 32) | fskit_random32();
    
    struct fskit_telldir_entry* tent = CALLOC_LIST( struct fskit_telldir_entry, 1 );
    if( tent == NULL ) {
        
        // emulate POSIX compliance
        return -EBADF;
    }
    
    // sanpshot read stream 
    fskit_dir_handle_wlock( dirh );
    
    tent->offset = offset;
    
    if( strlen(dirh->curr_name) > 0 ) {
        strcpy( tent->name, dirh->curr_name );
    }
    else {
        memset( tent->name, 0, FSKIT_FILESYSTEM_NAMEMAX );
    }
    
    // insert...
    struct fskit_telldir_entry* tmp = dirh->telldir_list;
    tent->next = tmp;
    dirh->telldir_list = tent;
    
    fskit_dir_handle_unlock( dirh );
    return offset;
}


// make the directory stream point to the beginning
void fskit_rewinddir( struct fskit_dir_handle* dirh ) {
    
    fskit_entry_set_itr read_itr;
    
    fskit_dir_handle_wlock( dirh );
    
    fskit_entry_set* start = fskit_entry_set_begin( &read_itr, dirh->dent->children );
    strncpy( dirh->curr_name, fskit_entry_set_name_at( start ), FSKIT_FILESYSTEM_NAMEMAX );
    
    fskit_dir_handle_unlock( dirh );
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
// returns a null-terminated list of directory entries
// on failure, it sets *err to one of the following:
// * -ENOMEM if no memory
// * -EDEADLK if there would be deadlock (this is a bug if it happens)
// * -EBADF if the directory hadndle is invalid
struct fskit_dir_entry** fskit_readdir( struct fskit_core* core, struct fskit_dir_handle* dirh, uint64_t num_children, uint64_t* num_read, int* err ) {

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
   
   struct fskit_dir_entry** dents = fskit_readdir_lowlevel( core, dirh, num_children, num_read, err );

   fskit_entry_unlock( dirh->dent );
   
   if( dents != NULL ) {
      
      // run the user's readdir
      rc = fskit_run_user_readdir( core, dirh->path, dirh->dent, dents, *num_read );
      if( rc != 0 ) {

         fskit_dir_entry_free_list( dents );
         *num_read = 0;
         *err = rc;
         
         dents = NULL;
      }
      else {
         
         // compactify the results--the user callback may have omitted some 
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
   }

   fskit_dir_handle_unlock( dirh );

   return dents;
}

// list a whole directory's data
// returns a null-terminated list of entries, and set *num_read to the number actually consumed 
// return NULL on error, and set *err
struct fskit_dir_entry** fskit_listdir( struct fskit_core* core, struct fskit_dir_handle* dirh, uint64_t* num_read, int* err ) {
   return fskit_readdir( core, dirh, UINT64_MAX, num_read, err );
}
