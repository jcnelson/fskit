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

#ifndef _FSKIT_ENTRY_H_
#define _FSKIT_ENTRY_H_

#include "common.h"
#include "debug.h"

#include <map>
#include <locale>

using namespace std;

// prototypes
struct fskit_entry;

// types
typedef pair<long, fskit_entry*> fskit_dirent;
typedef vector<fskit_dirent> fskit_entry_set;

#define FSKIT_ENTRY_TYPE_DEAD         0
#define FSKIT_ENTRY_TYPE_FILE         1
#define FSKIT_ENTRY_TYPE_DIR          2
#define FSKIT_ENTRY_TYPE_FIFO         3
#define FSKIT_ENTRY_TYPE_SOCK         4
#define FSKIT_ENTRY_TYPE_CHR          5
#define FSKIT_ENTRY_TYPE_BLK          6
#define FSKIT_ENTRY_TYPE_LNK          7

// root's UID
#define FSKIT_ROOT_USER_ID                0

// permissions checks
#define FSKIT_ENTRY_IS_READABLE( mode, node_user, node_group, user, group ) ((user) == FSKIT_ROOT_USER_ID || ((mode) & S_IROTH) || ((node_group) == (group) && ((mode) & S_IRGRP)) || ((node_user) == (user) && ((mode) & S_IRUSR)))
#define FSKIT_ENTRY_IS_DIR_READABLE( mode, node_user, node_group, user, group ) ((user) == FSKIT_ROOT_USER_ID || ((mode) & S_IXOTH) || ((node_group) == (group) && ((mode) & S_IXGRP)) || ((node_user) == (user) && ((mode) & S_IXUSR)))
#define FSKIT_ENTRY_IS_WRITEABLE( mode, node_user, node_group, user, group ) (((user) == FSKIT_ROOT_USER_ID || (mode) & S_IWOTH) || ((node_group) == (group) && ((mode) & S_IWGRP)) || ((node_user) == (user) && ((mode) & S_IWUSR)))
#define FSKIT_ENTRY_IS_EXECUTABLE( mode, node_user, node_group, user, group ) FSKIT_IS_DIR_READABLE( mode, node_user, node_group, user, group )


// fskit inode structure
struct fskit_entry {
   uint64_t file_id;             // inode number
   uint8_t type;                 // type of inode
   char* name;
   
   uint64_t owner;
   uint64_t group;
   
   mode_t mode;
   
   int64_t ctime_sec;
   int32_t ctime_nsec;
   
   int64_t mtime_sec;
   int32_t mtime_nsec;
   
   int64_t atime_sec;
   int32_t atime_nsec;
   
   int32_t open_count;
   int32_t link_count;
   
   bool deletion_in_progress;   // set to true if this node is in the process of being unlinked 
   
   // if this is a directory, this is allocated and points to a fskit_entry_set 
   fskit_entry_set* children;
   
   // lock governing access to this structure
   pthread_rwlock_t lock;
   
   // application-defined entry data 
   void* app_data;
   
   // if this is a special file, this is the device major/minor number 
   dev_t dev;
};

// fskit file handle 
struct fskit_file_handle {
   struct fskit_entry* fent;
   
   char* path;
   int flags;
   uint64_t file_id;

   // lock governing access to this structure
   pthread_rwlock_t lock;
   
   // application-defined data 
   void* app_data;
};

// fskit directory handle 
struct fskit_dir_handle {
   struct fskit_entry* dent;
      
   char* path;
   uint64_t file_id;
   
   // lock governing access to this structure
   pthread_rwlock_t lock;
   
   // application-defined data 
   void* app_data;
};

// fskit dir entry 
struct fskit_dir_entry {
   uint8_t type;        // type of file
   uint64_t file_id;    // file ID
   char* name;          // name of file
};

typedef uint64_t (*fskit_inode_alloc_t)( struct fskit_entry*, struct fskit_entry*, void* );
typedef int (*fskit_inode_free_t)( uint64_t, void* );

// fskit core structure 
struct fskit_core {
   
   struct fskit_entry root;
   
   // functions to allocate/deallocate inodes 
   fskit_inode_alloc_t fskit_inode_alloc;
   fskit_inode_free_t fskit_inode_free;
   
   // application-defined fs-wide data
   void* app_fs_data;
   
   // lock governing access to this structure
   pthread_rwlock_t lock;
};

// core management 
int fskit_core_init( struct fskit_core* core, void* app_data, void* root_app_data );
int fskit_core_destroy( struct fskit_core* core, void** app_fs_data );

// core callbacks 
int fskit_core_inode_alloc_cb( struct fskit_core* core, fskit_inode_alloc_t inode_alloc );
int fskit_core_inode_free_cb( struct fskit_core* core, fskit_inode_free_t inode_free );

// core methods 
uint64_t fskit_core_inode_alloc( struct fskit_core* core, struct fskit_entry* parent, struct fskit_entry* child );
int fskit_core_inode_free( struct fskit_core* core, uint64_t inode );
struct fskit_entry* fskit_core_resolve_root( struct fskit_core* core, bool writelock );

// memory management 
int fskit_entry_init_lowlevel( struct fskit_entry* fent, uint8_t type, uint64_t file_id, char const* name, uint64_t owner, uint64_t group, mode_t mode, void* app_data );
int fskit_entry_init_common( struct fskit_entry* fent, uint8_t type, uint64_t file_id, char const* name, uint64_t owner, uint64_t group, mode_t mode, void* app_data );
int fskit_entry_init_file( struct fskit_entry* fent, uint64_t file_id, char const* name, uint64_t owner, uint64_t group, mode_t mode, void* app_data );
int fskit_entry_init_dir( struct fskit_entry* fent, uint64_t file_id, char const* name, uint64_t owner, uint64_t group, mode_t mode, void* app_data );
int fskit_entry_init_fifo( struct fskit_entry* fent, uint64_t file_id, char const* name, uint64_t owner, uint64_t group, mode_t mode, void* app_data );
int fskit_entry_init_sock( struct fskit_entry* fent, uint64_t file_id, char const* name, uint64_t owner, uint64_t group, mode_t mode, void* app_data );
int fskit_entry_init_chr( struct fskit_entry* fent, uint64_t file_id, char const* name, uint64_t owner, uint64_t group, mode_t mode, dev_t dev, void* app_data );
int fskit_entry_init_blk( struct fskit_entry* fent, uint64_t file_id, char const* name, uint64_t owner, uint64_t group, mode_t mode, dev_t dev, void* app_data );
int fskit_entry_destroy( struct fskit_entry* fent, bool needlock );

// entry sets
void fskit_entry_set_insert( fskit_entry_set* set, char const* name, struct fskit_entry* child );
void fskit_entry_set_insert_hash( fskit_entry_set* set, long hash, struct fskit_entry* child );
struct fskit_entry* fskit_entry_set_find_name( fskit_entry_set* set, char const* name );
struct fskit_entry* fskit_entry_set_find_hash( fskit_entry_set* set, long nh );
bool fskit_entry_set_remove( fskit_entry_set* set, char const* name );
bool fskit_entry_set_remove_hash( fskit_entry_set* set, long nh );
bool fskit_entry_set_replace( fskit_entry_set* set, char const* name, struct fskit_entry* replacement );
unsigned int fskit_entry_set_count( fskit_entry_set* set );
struct fskit_entry* fskit_entry_set_get( fskit_entry_set::iterator* itr );
long fskit_entry_set_get_name_hash( fskit_entry_set::iterator* itr );

// locking 
int fskit_entry_rlock2( struct fskit_entry* fent, char const* from_str, int line_no );
int fskit_entry_wlock2( struct fskit_entry* fent, char const* from_str, int line_no );
int fskit_entry_unlock2( struct fskit_entry* fent, char const* from_str, int line_no );

#define fskit_entry_rlock( fent ) fskit_entry_rlock2( fent, __FILE__, __LINE__ )
#define fskit_entry_wlock( fent ) fskit_entry_wlock2( fent, __FILE__, __LINE__ )
#define fskit_entry_unlock( fent ) fskit_entry_unlock2( fent, __FILE__, __LINE__ )

int fskit_file_handle_rlock( struct fskit_file_handle* fh );
int fskit_file_handle_wlock( struct fskit_file_handle* fh );
int fskit_file_handle_unlock( struct fskit_file_handle* fh );

int fskit_dir_handle_rlock( struct fskit_dir_handle* dh );
int fskit_dir_handle_wlock( struct fskit_dir_handle* dh );
int fskit_dir_handle_unlock( struct fskit_dir_handle* dh );

int fskit_core_rlock( struct fskit_core* core );
int fskit_core_wlock( struct fskit_core* core );
int fskit_core_unlock( struct fskit_core* core );

// low-level linking and unlinking 
int fskit_entry_attach_lowlevel( struct fskit_entry* parent, struct fskit_entry* child );
int fskit_entry_detach_lowlevel( struct fskit_entry* parent, struct fskit_entry* child );

#endif