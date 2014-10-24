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
#include <queue>
#include <locale>

using namespace std;

// prototypes
struct fskit_entry;

// types
typedef pair<long, fskit_entry*> fskit_dirent;
typedef vector<fskit_dirent> fskit_entry_set;

typedef map<string, string> fskit_xattr_set;

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
#define FSKIT_ENTRY_IS_EXECUTABLE( mode, node_user, node_group, user, group ) FSKIT_ENTRY_IS_DIR_READABLE( mode, node_user, node_group, user, group )


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
   
   off_t size;          // number of bytes in this file
   
   bool deletion_in_progress;   // set to true if this node is in the process of being unlinked 
   
   // if this is a directory, this is allocated and points to a fskit_entry_set 
   fskit_entry_set* children;
   
   // application-defined entry data 
   void* app_data;
   
   // if this is a special file, this is the device major/minor number 
   dev_t dev;
   
   // lock governing access to the above structure fields
   pthread_rwlock_t lock;
   
   // extended attributes 
   fskit_xattr_set* xattrs;
   
   // lock governing access to xattrs
   pthread_rwlock_t xattrs_lock;
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

// type definitions for functions to allocate and free inodes
typedef uint64_t (*fskit_inode_alloc_t)( struct fskit_entry*, struct fskit_entry*, void* );
typedef int (*fskit_inode_free_t)( uint64_t, void* );

// prototype route 
struct fskit_path_route;

typedef vector< struct fskit_path_route > fskit_route_list_t;           // list of routes to try
typedef map< int, fskit_route_list_t > fskit_route_table_t;             // map route types to route lists

// fskit core structure 
struct fskit_core {
   
   // root inode
   struct fskit_entry root;
   
   // functions to allocate/deallocate inodes 
   fskit_inode_alloc_t fskit_inode_alloc;
   fskit_inode_free_t fskit_inode_free;
   
   // application-defined fs-wide data
   void* app_fs_data;
   
   // number of files and directories that exist 
   uint64_t num_files;
   
   // lock governing access to the above fields of this structure
   pthread_rwlock_t lock;
   
   /////////////////////////////////////////////////
   
   // path routes, indexed by FSKIT_ROUTE_MATCH_*
   fskit_route_table_t* routes;
   
   // lock governing access to the above fields of this structure
   pthread_rwlock_t route_lock;
};


// fskit detach context 
struct fskit_detach_ctx {
   
   queue<struct fskit_entry*>* destroy_queue;
   queue<char*>* destroy_paths;
};

// core management 
int fskit_core_init( struct fskit_core* core, void* app_data );
int fskit_core_destroy( struct fskit_core* core, void** app_fs_data );

// core callbacks 
int fskit_core_inode_alloc_cb( struct fskit_core* core, fskit_inode_alloc_t inode_alloc );
int fskit_core_inode_free_cb( struct fskit_core* core, fskit_inode_free_t inode_free );

// core methods 
uint64_t fskit_core_inode_alloc( struct fskit_core* core, struct fskit_entry* parent, struct fskit_entry* child );
int fskit_core_inode_free( struct fskit_core* core, uint64_t inode );
struct fskit_entry* fskit_core_resolve_root( struct fskit_core* core, bool writelock );

void* fskit_core_get_user_data( struct fskit_core* core );

// memory management 
int fskit_entry_init_lowlevel( struct fskit_entry* fent, uint8_t type, uint64_t file_id, char const* name, uint64_t owner, uint64_t group, mode_t mode );
int fskit_entry_init_common( struct fskit_entry* fent, uint8_t type, uint64_t file_id, char const* name, uint64_t owner, uint64_t group, mode_t mode );
int fskit_entry_init_file( struct fskit_entry* fent, uint64_t file_id, char const* name, uint64_t owner, uint64_t group, mode_t mode );
int fskit_entry_init_dir( struct fskit_entry* fent, uint64_t file_id, char const* name, uint64_t owner, uint64_t group, mode_t mode );
int fskit_entry_init_fifo( struct fskit_entry* fent, uint64_t file_id, char const* name, uint64_t owner, uint64_t group, mode_t mode );
int fskit_entry_init_sock( struct fskit_entry* fent, uint64_t file_id, char const* name, uint64_t owner, uint64_t group, mode_t mode );
int fskit_entry_init_chr( struct fskit_entry* fent, uint64_t file_id, char const* name, uint64_t owner, uint64_t group, mode_t mode, dev_t dev );
int fskit_entry_init_blk( struct fskit_entry* fent, uint64_t file_id, char const* name, uint64_t owner, uint64_t group, mode_t mode, dev_t dev );

// destruction
int fskit_run_user_detach( struct fskit_core* core, char const* path, struct fskit_entry* fent );
int fskit_entry_destroy( struct fskit_core* core, struct fskit_entry* fent, bool needlock );
int fskit_entry_try_destroy( struct fskit_core* core, char const* fs_path, struct fskit_entry* fent );
int fskit_detach_all( struct fskit_core* core, char const* root_path, fskit_entry_set* dir_children );
int fskit_detach_all_ex( struct fskit_core* core, char const* root_path, fskit_entry_set* dir_children, struct fskit_detach_ctx* ctx );
int fskit_detach_ctx_init( struct fskit_detach_ctx* ctx );
int fskit_detach_ctx_free( struct fskit_detach_ctx* ctx );

// entry sets
long fskit_entry_name_hash( char const* name );
int fskit_entry_set_insert( fskit_entry_set* set, char const* name, struct fskit_entry* child );
int fskit_entry_set_insert_hash( fskit_entry_set* set, long hash, struct fskit_entry* child );
struct fskit_entry* fskit_entry_set_find_name( fskit_entry_set* set, char const* name );
struct fskit_entry* fskit_entry_set_find_hash( fskit_entry_set* set, long nh );
bool fskit_entry_set_remove( fskit_entry_set* set, char const* name );
bool fskit_entry_set_remove_hash( fskit_entry_set* set, long nh );
bool fskit_entry_set_replace( fskit_entry_set* set, char const* name, struct fskit_entry* replacement );
unsigned int fskit_entry_set_count( fskit_entry_set* set );
struct fskit_entry* fskit_entry_set_get( fskit_entry_set::iterator* itr );
long fskit_entry_set_get_name_hash( fskit_entry_set::iterator* itr );
long fskit_entry_set_name_hash_at( fskit_entry_set* set, uint64_t i );
struct fskit_entry* fskit_entry_set_child_at( fskit_entry_set* set, uint64_t i );

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

int fskit_core_route_rlock( struct fskit_core* core );
int fskit_core_route_wlock( struct fskit_core* core );
int fskit_core_route_unlock( struct fskit_core* core );

int fskit_xattr_rlock( struct fskit_entry* fent );
int fskit_xattr_wlock( struct fskit_entry* fent );
int fskit_xattr_unlock( struct fskit_entry* fent );

// low-level linking and unlinking 
int fskit_entry_attach_lowlevel( struct fskit_entry* parent, struct fskit_entry* child );
int fskit_entry_detach_lowlevel( struct fskit_entry* parent, struct fskit_entry* child );

// getters 
uint64_t fskit_entry_get_file_id( struct fskit_entry* ent );
void* fskit_entry_get_user_data( struct fskit_entry* ent );

char* fskit_file_handle_get_path( struct fskit_file_handle* fh );
struct fskit_entry* fskit_file_handle_get_entry( struct fskit_file_handle* fh );
void* fskit_file_handle_get_user_data( struct fskit_file_handle* fh );

char* fskit_dir_handle_get_path( struct fskit_file_handle* fh );
struct fskit_entry* fskit_dir_handle_get_entry( struct fskit_file_handle* fh );
void* fskit_dir_handle_get_user_data( struct fskit_file_handle* fh );

// setters 
int fskit_entry_set_user_data( struct fskit_entry* ent, void* app_data );

// accounting 
uint64_t fskit_file_count_update( struct fskit_core* core, int change );

#endif