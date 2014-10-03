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

#include "debug.h"
#include "entry.h"
#include "path.h"
#include "random.h"
#include "route.h"

// hash a name/path
long fskit_entry_name_hash( char const* name ) {
   locale loc;
   const collate<char>& coll = use_facet<collate<char> >(loc);
   return coll.hash( name, name + strlen(name) );
}

// insert a child entry into an fskit_entry_set
void fskit_entry_set_insert( fskit_entry_set* set, char const* name, struct fskit_entry* child ) {
   long nh = fskit_entry_name_hash( name );
   return fskit_entry_set_insert_hash( set, nh, child );
}

// insert a child entry into an fskit_entry_set
void fskit_entry_set_insert_hash( fskit_entry_set* set, long hash, struct fskit_entry* child ) {
   for( unsigned int i = 0; i < set->size(); i++ ) {
      if( set->at(i).second == NULL ) {
         set->at(i).second = child;
         set->at(i).first = hash;
         return;
      }
   }

   fskit_dirent dent( hash, child );
   set->push_back( dent );
}


// find a child entry in a fskit_entry_set
struct fskit_entry* fskit_entry_set_find_name( fskit_entry_set* set, char const* name ) {
   long nh = fskit_entry_name_hash( name );
   return fskit_entry_set_find_hash( set, nh );
}


// find a child entry in an fs_entry set
struct fskit_entry* fskit_entry_set_find_hash( fskit_entry_set* set, long nh ) {
   for( unsigned int i = 0; i < set->size(); i++ ) {
      if( set->at(i).first == nh ) {
         return set->at(i).second;
      }
   }
   return NULL;
}


// remove a child entry from an fskit_entry_set
bool fskit_entry_set_remove( fskit_entry_set* set, char const* name ) {
   long nh = fskit_entry_name_hash( name );
   return fskit_entry_set_remove_hash( set, nh );
}


// remove a child entry from an fskit_entry_set
bool fskit_entry_set_remove_hash( fskit_entry_set* set, long nh ) {
   bool removed = false;
   for( unsigned int i = 0; i < set->size(); i++ ) {
      if( set->at(i).first == nh ) {
         // invalidate this
         set->at(i).second = NULL;
         set->at(i).first = 0;
         removed = true;
         break;
      }
   }

   return removed;
}


// replace an entry
bool fskit_entry_set_replace( fskit_entry_set* set, char const* name, struct fskit_entry* replacement ) {
   long nh = fskit_entry_name_hash( name );
   for( unsigned int i = 0; i < set->size(); i++ ) {
      if( set->at(i).first == nh ) {
         (*set)[i].second = replacement;
         return true;
      }
   }
   return false;
}


// count the number of entries in an fskit_entry_set
unsigned int fskit_entry_set_count( fskit_entry_set* set ) {
   unsigned int ret = 0;
   for( unsigned int i = 0; i < set->size(); i++ ) {
      if( set->at(i).second != NULL )
         ret++;
   }
   return ret;
}

// dereference an iterator to an fskit_entry_set member
struct fskit_entry* fskit_entry_set_get( fskit_entry_set::iterator* itr ) {
   return (*itr)->second;
}

// dereference an iterator to an fskit_entry_set member
long fskit_entry_set_get_name_hash( fskit_entry_set::iterator* itr ) {
   return (*itr)->first;
}

// get the ith child (or NULL if the request is off the end of the set)
struct fskit_entry* fskit_entry_set_child_at( fskit_entry_set* set, uint64_t i ) {
   
   if( i >= set->size() ) {
      return NULL;
   }
   else {
      return set->at(i).second;
   }
}

// get the ith child's name hash (or zero if it's off the end of the set)
long fskit_entry_set_name_hash_at( fskit_entry_set* set, uint64_t i ) {
   
   if( i >= set->size() ) {
      return 0;
   }
   else {
      return set->at(i).first;
   }
}

// attach an entry as a child directly
// both fskit_entry structures must be write-locked
int fskit_entry_attach_lowlevel( struct fskit_entry* parent, struct fskit_entry* fent ) {
   
   fent->link_count++;

   struct timespec ts;
   clock_gettime( CLOCK_REALTIME, &ts );
   parent->mtime_sec = ts.tv_sec;
   parent->mtime_nsec = ts.tv_nsec;

   fskit_entry_set_insert( parent->children, fent->name, fent );
   return 0;
}

// detach an entry from a parent.
// both entries must be write-locked.
// child's link count will be decremented
// the child will not be destroyed even if its link count reaches zero; the caller must take care of that.
int fskit_entry_detach_lowlevel( struct fskit_entry* parent, struct fskit_entry* child ) {

   if( parent == child ) {
      // tried to detach .
      return -ENOTEMPTY;
   }

   if( child == NULL ) {
      // no entry found
      return -ENOENT;
   }
   
   if( child->link_count == 0 ) {
      // child is invalid
      return -ENOENT;
   }

   // if the child is a directory, and it's not empty, then don't proceed
   if( child->type == FSKIT_ENTRY_TYPE_DIR && fskit_entry_set_count( child->children ) > 2 ) {
      // not empty
      return -ENOTEMPTY;
   }

   // unlink
   fskit_entry_set_remove( parent->children, child->name );
   
   struct timespec ts;
   clock_gettime( CLOCK_REALTIME, &ts );
   parent->mtime_sec = ts.tv_sec;
   parent->mtime_nsec = ts.tv_nsec;
   
   child->link_count--;
   
   return 0;
}

// change the number of files 
uint64_t fskit_file_count_update( struct fskit_core* core, int change ) {
   
   fskit_core_wlock( core );
   
   core->num_files += change;
   uint64_t num_files = core->num_files;
   
   fskit_core_unlock( core );
   
   return num_files;
}

// default inode allocator: pick a random 64-bit number 
static uint64_t fskit_default_inode_alloc( struct fskit_entry* parent, struct fskit_entry* child_to_receive_inode, void* ignored ) {
   uint64_t upper = fskit_random32();
   uint64_t lower = fskit_random32();
   
   return (upper << 32) | lower;
}

// default inode releaser; does nothing 
static int fskit_default_inode_free( uint64_t inode, void* ignored ) {
   return 0;
}

// initialize the fskit core 
int fskit_core_init( struct fskit_core* core, void* app_fs_data ) {
   
   int rc = 0;
   
   rc = fskit_entry_init_dir( &core->root, 0, "/", 0, 0, 0755 );
   if( rc != 0 ) {
      errorf("fskit_entry_init_dir(/) rc = %d\n", rc );
      return rc;
   }
   
   // root is linked to itself
   core->root.link_count = 1;
   fskit_entry_set_insert( core->root.children, ".", &core->root );
   fskit_entry_set_insert( core->root.children, "..", &core->root );
   
   core->app_fs_data = app_fs_data;
   
   core->fskit_inode_alloc = fskit_default_inode_alloc;
   core->fskit_inode_free = fskit_default_inode_free;
   
   core->routes = new fskit_route_table_t();
   
   pthread_rwlock_init( &core->lock, NULL );
   pthread_rwlock_init( &core->route_lock, NULL );
   
   return 0;
}


// free a list of routes 
static int fskit_route_list_free( fskit_route_list_t* route_list ) {
   
   for( unsigned int i = 0; i < route_list->size(); i++ ) {
      
      fskit_path_route_free( &route_list->at(i) );
   }
   
   route_list->clear();
   
   return 0;
}


// free a route table 
static int fskit_route_table_free( fskit_route_table_t* route_table ) {
   
   for( fskit_route_table_t::iterator itr = route_table->begin(); itr != route_table->end(); itr++ ) {
      
      fskit_route_list_free( &itr->second );
   }
   
   route_table->clear();
   return 0;
}

// destroy the fskit core
// core must be write-locked
// return the app_fs_data from core upon distruction
int fskit_core_destroy( struct fskit_core* core, void** app_fs_data ) {
   
   void* fs_data = core->app_fs_data;
   
   core->app_fs_data = NULL;
   
   fskit_entry_destroy( core, &core->root, true );
   
   fskit_route_table_free( core->routes );
   delete core->routes;
   
   pthread_rwlock_unlock( &core->lock );
   pthread_rwlock_destroy( &core->lock );
   pthread_rwlock_destroy( &core->route_lock );
   
   *app_fs_data = fs_data;
   
   memset( core, 0, sizeof(struct fskit_core) );
   
   return 0;
}

// unlink a directory's immediate children and subsequent descendants
// run any detach route callbacks.
// return 0 on success
// return -ENOMEM if out of memory
// NOTE: if -ENOMEM is encountered, this method will fail fast and return.
// This is because it will be unable to safely run user-defined routes.
// If this occurs, free up some memory and call this method again with the same detach context, but NULL for dir_children
int fskit_detach_all_ex( struct fskit_core* core, char const* root_path, fskit_entry_set* dir_children, struct fskit_detach_ctx* ctx ) {
   
   int rc = 0;
   
   // queue immediate children for destruction
   if( dir_children != NULL ) {
      for( fskit_entry_set::iterator itr = dir_children->begin(); itr != dir_children->end(); ) {
         
         struct fskit_entry* child = fskit_entry_set_get( &itr );

         if( child == NULL ) {
            itr = dir_children->erase( itr );
            continue;
         }

         long fent_name_hash = fskit_entry_set_get_name_hash( &itr );

         if( fent_name_hash == fskit_entry_name_hash( "." ) || fent_name_hash == fskit_entry_name_hash( ".." ) ) {
            itr++;
            continue;
         }
         
         fskit_entry_wlock( child );       // NOTE: don't care if this fails with EDEADLK, since that means the caller owns the lock anyway
         
         char* child_path = fskit_fullpath( root_path, child->name, NULL );
         if( child_path == NULL ) {
            
            return -ENOMEM;
         }
         
         ctx->destroy_queue->push( child );
         ctx->destroy_paths->push( child_path );

         itr = dir_children->erase( itr );
      }
   }
   
   while( ctx->destroy_queue->size() > 0 ) {
      
      struct fskit_entry* fent = ctx->destroy_queue->front();
      char* fent_path = ctx->destroy_paths->front();
      
      ctx->destroy_queue->pop();
      ctx->destroy_paths->pop();

      int old_type = fent->type;
      
      // unlink this entry
      fent->type = FSKIT_ENTRY_TYPE_DEAD;
      fent->link_count = 0;

      // destroy the entry if it's not a directory and no longer referenced
      if( old_type != FSKIT_ENTRY_TYPE_DIR ) {
         
         fent->link_count = 0;
         
         rc = fskit_entry_try_destroy( core, fent_path, fent );
         if( rc > 0 ) {
            
            // destroyed!
            safe_free( fent );
            rc = 0;
         }
         
         safe_free( fent_path );
      }

      else {
         
         // unlink this directory
         fskit_entry_set* children = fent->children;
         fent->children = NULL;
         
         fent->link_count = 0;

         rc = fskit_entry_try_destroy( core, fent_path, fent );
         if( rc > 0 ) {
            
            // destroyed!
            safe_free( fent );
            rc = 0;
         }

         for( fskit_entry_set::iterator itr = children->begin(); itr != children->end(); itr++ ) {
            
            struct fskit_entry* child = fskit_entry_set_get( &itr );

            if( child == NULL ) {
               continue;
            }
            
            long fent_name_hash = fskit_entry_set_get_name_hash( &itr );

            if( fent_name_hash == fskit_entry_name_hash( "." ) || fent_name_hash == fskit_entry_name_hash( ".." ) ) {
               continue;
            }
            
            fskit_entry_wlock( child );         // NOTE: don't care if this fails with -EDEADLK, since that means the calling thread owns the lock anyway
            
            char* child_path = fskit_fullpath( fent_path, child->name, NULL );
            if( child_path == NULL ) {
               
               return -ENOMEM;
            }
            else {
               // queue for destruction
               ctx->destroy_queue->push( child );
               ctx->destroy_paths->push( child_path );
            }
         }

         delete children;
         
         safe_free( fent_path );
      }
   }
   
   // if all went well, then ctx's queues will be empty
   
   return 0;
}


// set up a detach context 
int fskit_detach_ctx_init( struct fskit_detach_ctx* ctx ) {
   
   ctx->destroy_queue = new queue<struct fskit_entry*>();
   ctx->destroy_paths = new queue<char*>();
   
   if( ctx->destroy_queue == NULL || ctx->destroy_paths == NULL ) {
      
      safe_delete( ctx->destroy_queue );
      safe_delete( ctx->destroy_paths );
      
      return -ENOMEM;
   }
   
   return 0;
}


// free a detach context 
int fskit_detach_ctx_free( struct fskit_detach_ctx* ctx ) {
   
   safe_delete( ctx->destroy_queue );
   safe_delete( ctx->destroy_paths );
   
   return 0;
}

// if you expect that fskit_detach_all_ex will succeed in one go, then you can use this helper function 
int fskit_detach_all( struct fskit_core* core, char const* root_path, fskit_entry_set* dir_children ) {
   
   struct fskit_detach_ctx ctx;
   int rc = 0;
   
   rc = fskit_detach_ctx_init( &ctx );
   if( rc != 0 ) {
      return rc;
   }
   
   while( true ) {
      rc = fskit_detach_all_ex( core, root_path, dir_children, &ctx );
      if( rc == 0 ) {
         break;
      }
      else if( rc == -ENOMEM ) {
         continue;
      }
      else {
         break;
      }
   }
   
   fskit_detach_ctx_free( &ctx );
   
   return rc;
}

// set the inode allocator
int fskit_core_inode_alloc_cb( struct fskit_core* core, fskit_inode_alloc_t inode_alloc ) {
   
   int rc = 0;
   
   rc = fskit_core_wlock( core );
   if( rc != 0 ) {
      return rc;
   }
   
   core->fskit_inode_alloc = inode_alloc;
   
   fskit_core_unlock( core );
   return 0;
}

// set the inode releaser 
int fskit_core_inode_free_cb( struct fskit_core* core, fskit_inode_free_t inode_free ) {
   
   int rc = 0;
   
   rc = fskit_core_wlock( core );
   if( rc != 0 ) {
      return rc;
   }
   
   core->fskit_inode_free = inode_free;
   
   fskit_core_unlock( core );
   return 0;
}


// get the next free inode 
// return 0 on error
uint64_t fskit_core_inode_alloc( struct fskit_core* core, struct fskit_entry* parent, struct fskit_entry* child ) {
   
   int rc = 0;
   
   rc = fskit_core_rlock( core );
   if( rc != 0 ) {
      return 0;
   }
   
   uint64_t next_inode = (*core->fskit_inode_alloc)( parent, child, core->app_fs_data );
   
   fskit_core_unlock( core );
   
   return next_inode;
}

// release an inode 
int fskit_core_inode_free( struct fskit_core* core, uint64_t inode ) {
   
   int rc = 0;
   
   rc = fskit_core_rlock( core );
   if( rc != 0 ) {
      return rc;
   }
   
   rc = (*core->fskit_inode_free)( inode, core->app_fs_data );
   
   fskit_core_unlock( core );
   
   return rc;
}

// get the root node 
struct fskit_entry* fskit_core_resolve_root( struct fskit_core* core, bool writelock ) {
   
   fskit_core_rlock( core );
   
   if( core->root.type == FSKIT_ENTRY_TYPE_DIR && !core->root.deletion_in_progress ) {
      // not dead 
      if( writelock ) {
         fskit_entry_rlock( &core->root );
      }
      else {
         fskit_entry_wlock( &core->root );
      }
      
      fskit_core_unlock( core );
      return &core->root;
   }
   else {
      fskit_core_unlock( core );
      return NULL;
   }
}

// initialize an fskit entry 
// this method does not fail.
int fskit_entry_init_lowlevel( struct fskit_entry* fent, uint8_t type, uint64_t file_id, char const* name, uint64_t owner, uint64_t group, mode_t mode ) {
   
   int rc = 0;
   
   memset( fent, 0, sizeof(struct fskit_entry) );
   
   fent->type = type;
   fent->file_id = file_id;
   fent->name = strdup(name);
   fent->owner = owner;
   fent->group = group;
   fent->mode = mode;
   
   return rc;
}


// common high-level initializer 
int fskit_entry_init_common( struct fskit_entry* fent, uint8_t type, uint64_t file_id, char const* name, uint64_t owner, uint64_t group, mode_t mode ) {
   
   int rc = 0;
   struct timespec now;
   
   rc = clock_gettime( CLOCK_REALTIME, &now );
   if( rc != 0 ) {
      errorf("clock_gettime rc = %d\n", rc );
      return rc;
   }
   
   fskit_entry_init_lowlevel( fent, type, file_id, name, owner, group, mode );
   
   fskit_entry_set_atime( fent, &now );
   fskit_entry_set_ctime( fent, &now );
   fskit_entry_set_mtime( fent, &now );
   
   return 0;
}

// high-level initializer: make a file 
int fskit_entry_init_file( struct fskit_entry* fent, uint64_t file_id, char const* name, uint64_t owner, uint64_t group, mode_t mode ) {
   
   int rc = 0;
   
   rc = fskit_entry_init_common( fent, FSKIT_ENTRY_TYPE_FILE, file_id, name, owner, group, mode );
   if( rc != 0 ) {
      errorf("fs_entry_init_common(%" PRIX64 " %s) rc = %d\n", file_id, name, rc );
      return rc;
   }
   
   // for files, no further initialization needed
   return 0;
}


// high-level initializer: make a directory 
int fskit_entry_init_dir( struct fskit_entry* fent, uint64_t file_id, char const* name, uint64_t owner, uint64_t group, mode_t mode ) {
   
   int rc = 0;
   
   rc = fskit_entry_init_common( fent, FSKIT_ENTRY_TYPE_DIR, file_id, name, owner, group, mode );
   if( rc != 0 ) {
      errorf("fs_entry_init_common(%" PRIX64 " %s) rc = %d\n", file_id, name, rc );
      return rc;
   }
   
   // set up children 
   fent->children = new fskit_entry_set();
   return 0;
}

// high-level initializer: make a fifo 
int fskit_entry_init_fifo( struct fskit_entry* fent, uint64_t file_id, char const* name, uint64_t owner, uint64_t group, mode_t mode ) {
   
   int rc = 0;
   
   rc = fskit_entry_init_common( fent, FSKIT_ENTRY_TYPE_FIFO, file_id, name, owner, group, mode );
   if( rc != 0 ) {
      errorf("fs_entry_init_common(%" PRIX64 " %s) rc = %d\n", file_id, name, rc );
      return rc;
   }
   
   // nothing more to do for now 
   return 0;
}

// high-level initializer: make a UNIX domain socket 
int fskit_entry_init_sock( struct fskit_entry* fent, uint64_t file_id, char const* name, uint64_t owner, uint64_t group, mode_t mode ) {
   
   int rc = 0;
   
   rc = fskit_entry_init_common( fent, FSKIT_ENTRY_TYPE_SOCK, file_id, name, owner, group, mode );
   if( rc != 0 ) {
      errorf("fs_entry_init_common(%" PRIX64 " %s) rc = %d\n", file_id, name, rc );
      return rc;
   }
   
   // nothing more to do for now 
   return 0;
}

// high-level initializer: make a character device 
int fskit_entry_init_chr( struct fskit_entry* fent, uint64_t file_id, char const* name, uint64_t owner, uint64_t group, mode_t mode, dev_t dev ) {

   int rc = 0;
   
   rc = fskit_entry_init_common( fent, FSKIT_ENTRY_TYPE_CHR, file_id, name, owner, group, mode );
   if( rc != 0 ) {
      errorf("fs_entry_init_common(%" PRIX64 " %s) rc = %d\n", file_id, name, rc );
      return rc;
   }
   
   fent->dev = dev;
   return 0;   
}

// high-level initializer: make a block device 
int fskit_entry_init_blk( struct fskit_entry* fent, uint64_t file_id, char const* name, uint64_t owner, uint64_t group, mode_t mode, dev_t dev ) {
   
   int rc = 0;
   
   rc = fskit_entry_init_common( fent, FSKIT_ENTRY_TYPE_BLK, file_id, name, owner, group, mode );
   if( rc != 0 ) {
      errorf("fs_entry_init_common(%" PRIX64 " %s) rc = %d\n", file_id, name, rc );
      return rc;
   }
   
   fent->dev = dev;
   return 0;
}

// run user-supplied route callback to destroy this fent's inode data 
int fskit_run_user_detach( struct fskit_core* core, char const* path, struct fskit_entry* fent ) {
   
   int rc = 0;
   int cbrc = 0;
   struct fskit_route_dispatch_args dargs;
   
   fskit_route_detach_args( &dargs, fent->app_data );
   
   rc = fskit_route_call_detach( core, path, fent, &dargs, &cbrc );
   
   if( rc == -EPERM || rc == -ENOSYS ) {
      // no routes 
      return 0;
   }
   
   else if( cbrc != 0 ) {
      // callback failed 
      return cbrc;
   }
   
   else {
      // callback succeded
      return 0;
   }
}


// destroy a fskit entry
// NOTE: fent must be write-locked if is attached to the filesystem, or needlock must be true
int fskit_entry_destroy( struct fskit_core* core, struct fskit_entry* fent, bool needlock ) {

   if( needlock ) {
      fskit_entry_wlock( fent );
   }

   // free common fields
   if( fent->name ) {
      safe_free( fent->name );
      fent->name = NULL;
   }

   if( fent->children ) {
      delete fent->children;
      fent->children = NULL;
   }

   fent->type = FSKIT_ENTRY_TYPE_DEAD;      // next thread to hold this lock knows this is a dead entry

   fskit_entry_unlock( fent );
   pthread_rwlock_destroy( &fent->lock );
   
   return 0;
}


// try to destroy an fskit entry, if it is unlinked and no longer open
// return 0 if not destroyed
// return 1 if destroyed
// return negative on error
// fent must be write-locked
// NOTE: even if this method returns an error code, the entry will be destroyed if it is no longer linked
int fskit_entry_try_destroy( struct fskit_core* core, char const* fs_path, struct fskit_entry* fent ) {
   
   int rc = 0;

   if( fent->link_count <= 0 && fent->open_count <= 0 ) {
      
      rc = fskit_run_user_detach( core, fs_path, fent );
      if( rc != 0 ) {
         errorf("fskit_run_user_detach(%s) rc = %d\n", fs_path, rc );
      }
      
      fskit_entry_destroy( core, fent, false );
      
      rc = 1;
   }

   return rc;
}


// lock a file for reading
int fskit_entry_rlock2( struct fskit_entry* fent, char const* from_str, int line_no ) {
   int rc = pthread_rwlock_rdlock( &fent->lock );
   if( rc == 0 ) {
      if( _debug_locks ) {
         dbprintf( "%p: %s, from %s:%d\n", fent, fent->name, from_str, line_no );
      }
   }
   else {
      errorf("pthread_rwlock_rdlock(%p) rc = %d (from %s:%d)\n", fent, rc, from_str, line_no );
   }

   return rc;
}

// lock a file for writing
int fskit_entry_wlock2( struct fskit_entry* fent, char const* from_str, int line_no ) {
   int rc = pthread_rwlock_wrlock( &fent->lock );
   if( fent->type == FSKIT_ENTRY_TYPE_DEAD ) {
      return -ENOENT;
   }
   
   if( rc == 0 ) {
      if( _debug_locks ) {
         dbprintf( "%p: %s, from %s:%d\n", fent, fent->name, from_str, line_no );
      }
   }
   else {
      errorf("pthread_rwlock_wrlock(%p) rc = %d (from %s:%d)\n", fent, rc, from_str, line_no );
   }

   return rc;
}

// unlock a file
int fskit_entry_unlock2( struct fskit_entry* fent, char const* from_str, int line_no ) {
   int rc = pthread_rwlock_unlock( &fent->lock );
   if( rc == 0 ) {
      if( _debug_locks ) {
         dbprintf( "%p: %s, from %s:%d\n", fent, fent->name, from_str, line_no );
      }
   }
   else {
      errorf("pthread_rwlock_unlock(%p) rc = %d (from %s:%d)\n", fent, rc, from_str, line_no );
   }

   return rc;
}

// lock a file handle for reading
int fskit_file_handle_rlock( struct fskit_file_handle* fh ) {
   return pthread_rwlock_rdlock( &fh->lock );
}

// lock a file handle for writing
int fskit_file_handle_wlock( struct fskit_file_handle* fh ) {
   return pthread_rwlock_wrlock( &fh->lock );
}

// unlock a file handle
int fskit_file_handle_unlock( struct fskit_file_handle* fh ) {
   return pthread_rwlock_unlock( &fh->lock );
}

// lock a directory handle for reading
int fskit_dir_handle_rlock( struct fskit_dir_handle* dh ) {
   return pthread_rwlock_rdlock( &dh->lock );
}

// lock a directory handle for writing
int fskit_dir_handle_wlock( struct fskit_dir_handle* dh ) {
   return pthread_rwlock_wrlock( &dh->lock );
}

// unlock a directory handle
int fskit_dir_handle_unlock( struct fskit_dir_handle* dh ) {
   return pthread_rwlock_unlock( &dh->lock );
}

// read-lock a filesystem core
int fskit_core_rlock( struct fskit_core* core ) {
   return pthread_rwlock_rdlock( &core->lock );
}

// write-lock a filesystem core
int fskit_core_wlock( struct fskit_core* core ) {
   return pthread_rwlock_wrlock( &core->lock );
}

// unlock a filesystem core
int fskit_core_unlock( struct fskit_core* core ) {
   return pthread_rwlock_unlock( &core->lock );
}

// read-lock routes 
int fskit_core_route_rlock( struct fskit_core* core ) {
   return pthread_rwlock_rdlock( &core->route_lock );
}

// write-lock routes 
int fskit_core_route_wlock( struct fskit_core* core ) {
   return pthread_rwlock_wrlock( &core->route_lock );
}

// unlock routes 
int fskit_core_route_unlock( struct fskit_core* core ) {
   return pthread_rwlock_unlock( &core->route_lock );
}


// set user data in an fskit_entry (which must be write-locked)
// return 0 always 
int fskit_entry_set_user_data( struct fskit_entry* ent, void* app_data ) {
   ent->app_data = app_data;
   return 0;
}
