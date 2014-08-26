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
#include "ops.h"

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
// both entries must be write-locked 
// this will destroy child if its open count is 0
int fskit_entry_detach_lowlevel( struct fskit_entry* parent, struct fskit_entry* child ) {

   if( parent == child ) {
      // tried to detach .
      return -ENOTEMPTY;
   }

   if( child == NULL ) {
      // no entry found
      return -ENOENT;
   }

   fskit_entry_wlock( child );

   if( child->link_count == 0 ) {
      // child is invalid
      fskit_entry_unlock( child );
      return -ENOENT;
   }

   // if the child is a directory, and it's not empty, then don't proceed
   if( child->type == FSKIT_ENTRY_TYPE_DIR && fskit_entry_set_count( child->children ) > 2 ) {
      // not empty
      fskit_entry_unlock( child );
      return -ENOTEMPTY;
   }

   // unlink
   fskit_entry_set_remove( parent->children, child->name );
   
   struct timespec ts;
   clock_gettime( CLOCK_REALTIME, &ts );
   parent->mtime_sec = ts.tv_sec;
   parent->mtime_nsec = ts.tv_nsec;

   int rc = 0;

   if( child->open_count == 0 ) {
   
      fskit_entry_destroy( child, false );
      free( child );
      child = NULL;
   }
   else {
      fskit_entry_unlock( child );
   }

   if( rc == 0 && child ) {
      child->link_count = 0;
   }

   return rc;
}

// default inode allocator: pick a random 64-bit number 
static uint64_t fskit_default_inode_alloc( struct fskit_entry* parent, struct fskit_entry* child_to_receive_inode, void* ignored ) {
   uint64_t upper = fskit_random32();
   uint64_t lower = fskit_random32();
   
   return (upper << 32) | lower;
}

// initialize the fskit core 
int fskit_core_init( struct fskit_core* core, void* app_fs_data, void* root_app_data ) {
   
   int rc = 0;
   
   rc = fskit_entry_init_dir( &core->root, 0, "/", 0, 0, 0755, root_app_data );
   if( rc != 0 ) {
      errorf("fskit_entry_init_dir(/) rc = %d\n", rc );
      return rc;
   }
   
   core->app_fs_data = app_fs_data;
   core->fskit_inode_alloc = fskit_default_inode_alloc;
   
   pthread_rwlock_init( &core->lock, NULL );
   
   return 0;
}


// destroy the fskit core
// core must be write-locked
// return the app_fs_data from core upon distruction
int fskit_core_destroy( struct fskit_core* core, void** app_fs_data ) {
   
   void* fs_data = core->app_fs_data;
   
   core->app_fs_data = NULL;
   
   fskit_entry_destroy( &core->root, true );
   
   pthread_rwlock_unlock( &core->lock );
   pthread_rwlock_destroy( &core->lock );
   
   *app_fs_data = fs_data;
   
   memset( core, 0, sizeof(struct fskit_core) );
   
   return 0;
}

// set the inode allocator
int fskit_core_inode_alloc( struct fskit_core* core, fskit_inode_alloc_t inode_alloc ) {
   
   fskit_core_wlock( core );
   
   core->fskit_inode_alloc = inode_alloc;
   
   fskit_core_unlock( core );
   return 0;
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
int fskit_entry_init_lowlevel( struct fskit_entry* fent, uint8_t type, uint64_t file_id, char const* name, uint64_t owner, uint64_t group, mode_t mode, void* app_data ) {
   
   int rc = 0;
   
   memset( fent, 0, sizeof(struct fskit_entry) );
   
   fent->type = type;
   fent->file_id = file_id;
   fent->name = strdup(name);
   fent->owner = owner;
   fent->group = group;
   fent->mode = mode;
   
   fent->app_data = app_data;
   
   return rc;
}


// common high-level initializer 
int fskit_entry_init_common( struct fskit_entry* fent, uint8_t type, uint64_t file_id, char const* name, uint64_t owner, uint64_t group, mode_t mode, void* app_data ) {
   
   int rc = 0;
   struct timespec now;
   
   rc = clock_gettime( CLOCK_REALTIME, &now );
   if( rc != 0 ) {
      errorf("clock_gettime rc = %d\n", rc );
      return rc;
   }
   
   fskit_entry_init_lowlevel( fent, type, file_id, name, owner, group, mode, app_data );
   
   fskit_entry_set_atime( fent, &now );
   fskit_entry_set_ctime( fent, &now );
   fskit_entry_set_mtime( fent, &now );
   
   return 0;
}

// high-level initializer: make a file 
int fskit_entry_init_file( struct fskit_entry* fent, uint64_t file_id, char const* name, uint64_t owner, uint64_t group, mode_t mode, void* app_data ) {
   
   int rc = 0;
   
   rc = fskit_entry_init_common( fent, FSKIT_ENTRY_TYPE_FILE, file_id, name, owner, group, mode, app_data );
   if( rc != 0 ) {
      errorf("fs_entry_init(%" PRIX64 " %s) rc = %d\n", file_id, name, rc );
      return rc;
   }
   
   // for files, no further initialization needed
   return 0;
}


// high-level initializer: make a directory 
int fskit_entry_init_dir( struct fskit_entry* fent, uint64_t file_id, char const* name, uint64_t owner, uint64_t group, mode_t mode, void* app_data ) {
   
   int rc = 0;
   
   rc = fskit_entry_init_common( fent, FSKIT_ENTRY_TYPE_DIR, file_id, name, owner, group, mode, app_data );
   if( rc != 0 ) {
      errorf("fs_entry_init(%" PRIX64 " %s) rc = %d\n", file_id, name, rc );
      return rc;
   }
   
   // set up children 
   fent->children = new fskit_entry_set();
   return 0;
}


// destroy a fskit entry
// NOTE: fent must be write-locked, or needlock must be true
int fskit_entry_destroy( struct fskit_entry* fent, bool needlock ) {

   dbprintf("destroy %" PRIX64 " (%s) (%p)\n", fent->file_id, fent->name, fent );

   // free common fields
   if( needlock ) {
      fskit_entry_wlock( fent );
   }

   if( fent->name ) {
      free( fent->name );
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


