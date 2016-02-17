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

#ifndef _FSKIT_ENTRY_H_
#define _FSKIT_ENTRY_H_

#include <fskit/common.h>
#include <fskit/debug.h>
#include <fskit/sglib.h>

#define FSKIT_FILESYSTEM_NAMEMAX 255

#define FSKIT_ENTRY_SET_ENTRY_CMP( s1, s2 ) (strcmp((s1)->name, (s2)->name))
#define FSKIT_XATTR_SET_ENTRY_CMP( x1, x2 ) (strcmp((x1)->name, (x2)->name))

// inode types
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
#define FSKIT_ENTRY_IS_DIR_SEARCHABLE( mode, node_user, node_group, user, group ) ((user) == FSKIT_ROOT_USER_ID || ((mode) & S_IXOTH) || ((node_group) == (group) && ((mode) & S_IXGRP)) || ((node_user) == (user) && ((mode) & S_IXUSR)))
#define FSKIT_ENTRY_IS_WRITEABLE( mode, node_user, node_group, user, group ) (((user) == FSKIT_ROOT_USER_ID || (mode) & S_IWOTH) || ((node_group) == (group) && ((mode) & S_IWGRP)) || ((node_user) == (user) && ((mode) & S_IWUSR)))
#define FSKIT_ENTRY_IS_EXECUTABLE( mode, node_user, node_group, user, group ) FSKIT_ENTRY_IS_DIR_SEARCHABLE( mode, node_user, node_group, user, group )

FSKIT_C_LINKAGE_BEGIN 

// entry set
struct fskit_entry_set_entry;
typedef struct fskit_entry_set_entry fskit_entry_set;
struct fskit_detach_ctx;

// fskit inode structure
struct fskit_entry;

// fskit file handle
struct fskit_file_handle;

// fskit directory handle
struct fskit_dir_handle;

// fskit dir entry
struct fskit_dir_entry {
   uint8_t type;        // type of file
   uint64_t file_id;    // file ID
   char name[FSKIT_FILESYSTEM_NAMEMAX+1];          // name of file
};

// type definitions for functions to allocate and free inodes
typedef uint64_t (*fskit_inode_alloc_t)( struct fskit_entry*, struct fskit_entry*, void* );
typedef int (*fskit_inode_free_t)( uint64_t, void* );

// routes
struct fskit_path_route;

// fskit core structure
struct fskit_core;

// entry set destruction
int fskit_detach_all( struct fskit_core* core, char const* root_path );
int fskit_detach_all_ex( struct fskit_core* core, char const* root_path, fskit_entry_set** dir_children, struct fskit_detach_ctx* ctx );

// core management
struct fskit_core* fskit_core_new();
int fskit_core_init( struct fskit_core* core, void* app_data );
int fskit_core_destroy( struct fskit_core* core, void** app_fs_data );
struct fskit_entry* fskit_core_get_root( struct fskit_core* core );

// core callbacks
int fskit_core_inode_alloc_cb( struct fskit_core* core, fskit_inode_alloc_t inode_alloc );
int fskit_core_inode_free_cb( struct fskit_core* core, fskit_inode_free_t inode_free );

// core methods
uint64_t fskit_core_inode_alloc( struct fskit_core* core, struct fskit_entry* parent, struct fskit_entry* child );
int fskit_core_inode_free( struct fskit_core* core, uint64_t inode );
struct fskit_entry* fskit_core_resolve_root( struct fskit_core* core, bool writelock );
void* fskit_core_get_user_data( struct fskit_core* core );

// lookup
struct fskit_entry* fskit_dir_find_by_name( struct fskit_entry* dir, char const* name );

// entry sets
SGLIB_DEFINE_RBTREE_PROTOTYPES( fskit_entry_set, left, right, color, FSKIT_ENTRY_SET_ENTRY_CMP );
typedef struct sglib_fskit_entry_set_iterator fskit_entry_set_itr;

fskit_entry_set* fskit_entry_set_new( struct fskit_entry* node, struct fskit_entry* parent );
int fskit_entry_set_free( fskit_entry_set* set );
int fskit_entry_set_insert( fskit_entry_set** set, char const* name, struct fskit_entry* child );
struct fskit_entry* fskit_entry_set_find_name( fskit_entry_set* set, char const* name );
fskit_entry_set* fskit_entry_set_find_itr( fskit_entry_set* set, char const* name );
bool fskit_entry_set_remove( fskit_entry_set** set, char const* name );
bool fskit_entry_set_replace( fskit_entry_set* set, char const* name, struct fskit_entry* replacement );
unsigned int fskit_entry_set_count( fskit_entry_set* set );

// xattr sets 
struct fskit_xattr_set_entry;
typedef struct fskit_xattr_set_entry fskit_xattr_set;

SGLIB_DEFINE_RBTREE_PROTOTYPES( fskit_xattr_set, left, right, color, FSKIT_XATTR_SET_ENTRY_CMP );
typedef struct sglib_fskit_xattr_set_iterator fskit_xattr_set_itr;

fskit_xattr_set* fskit_xattr_set_new(void);
int fskit_xattr_set_free( fskit_xattr_set* xattrs );
int fskit_xattr_set_insert( fskit_xattr_set** xattrs, char const* name, char const* value, size_t value_len, int flags );
char const* fskit_xattr_set_find( fskit_xattr_set* xattrs, char const* name, size_t* len );
bool fskit_xattr_set_remove( fskit_xattr_set** xattrs, char const* name );
unsigned int fskit_xattr_set_count( fskit_xattr_set* xattrs );
char const* fskit_xattr_set_name( fskit_xattr_set* xattrs );
char const* fskit_xattr_set_value( fskit_xattr_set* xattrs );
size_t fskit_xattr_set_value_len( fskit_xattr_set* xattrs );

fskit_xattr_set* fskit_xattr_set_begin( fskit_xattr_set_itr* itr, fskit_xattr_set* xattrs );
fskit_xattr_set* fskit_xattr_set_next( fskit_xattr_set_itr* itr );


// iteration 
fskit_entry_set* fskit_entry_set_begin( fskit_entry_set_itr* itr, fskit_entry_set* dirents );
fskit_entry_set* fskit_entry_set_next( fskit_entry_set_itr* itr );
char const* fskit_entry_set_name_at( fskit_entry_set* dp );
struct fskit_entry* fskit_entry_set_child_at( fskit_entry_set* dp );

// initialization
struct fskit_entry* fskit_entry_new(void);
int fskit_entry_init_lowlevel( struct fskit_entry* fent, uint8_t type, uint64_t file_id, uint64_t owner, uint64_t group, mode_t mode );
int fskit_entry_init_common( struct fskit_entry* fent, uint8_t type, uint64_t file_id, uint64_t owner, uint64_t group, mode_t mode );
int fskit_entry_init_file( struct fskit_entry* fent, uint64_t file_id, uint64_t owner, uint64_t group, mode_t mode );
int fskit_entry_init_dir( struct fskit_entry* fent, struct fskit_entry* parent, uint64_t file_id, uint64_t owner, uint64_t group, mode_t mode );
int fskit_entry_init_fifo( struct fskit_entry* fent, uint64_t file_id, uint64_t owner, uint64_t group, mode_t mode );
int fskit_entry_init_sock( struct fskit_entry* fent, uint64_t file_id, uint64_t owner, uint64_t group, mode_t mode );
int fskit_entry_init_chr( struct fskit_entry* fent, uint64_t file_id, uint64_t owner, uint64_t group, mode_t mode, dev_t dev );
int fskit_entry_init_blk( struct fskit_entry* fent, uint64_t file_id, uint64_t owner, uint64_t group, mode_t mode, dev_t dev );
int fskit_entry_init_symlink( struct fskit_entry* fent, uint64_t file_id, char const* linkpath );

// destruction
int fskit_entry_destroy( struct fskit_core* core, struct fskit_entry* fent, bool needlock );
int fskit_entry_try_destroy_and_free( struct fskit_core* core, char const* fs_path, struct fskit_entry* parent, struct fskit_entry* fent );
struct fskit_detach_ctx* fskit_detach_ctx_new();
int fskit_detach_ctx_init( struct fskit_detach_ctx* ctx );
int fskit_detach_ctx_free( struct fskit_detach_ctx* ctx );
int fskit_entry_tag_garbage( struct fskit_entry* ent, fskit_entry_set** children );

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

int fskit_core_rlock2( struct fskit_core* core, char const* from_str, int line_no );
int fskit_core_wlock2( struct fskit_core* core, char const* from_str, int line_no );
int fskit_core_unlock2( struct fskit_core* core, char const* from_str, int line_no );

#define fskit_core_rlock( core ) fskit_core_rlock2( core, __FILE__, __LINE__ )
#define fskit_core_wlock( core ) fskit_core_wlock2( core, __FILE__, __LINE__ )
#define fskit_core_unlock( core ) fskit_core_unlock2( core, __FILE__, __LINE__ )

int fskit_core_route_rlock( struct fskit_core* core );
int fskit_core_route_wlock( struct fskit_core* core );
int fskit_core_route_unlock( struct fskit_core* core );

int fskit_xattr_rlock( struct fskit_entry* fent );
int fskit_xattr_wlock( struct fskit_entry* fent );
int fskit_xattr_unlock( struct fskit_entry* fent );

// low-level linking and unlinking
int fskit_entry_attach_lowlevel( struct fskit_entry* parent, struct fskit_entry* child, char const* name );
int fskit_entry_detach_lowlevel( struct fskit_entry* parent, char const* name );

// inode getters
uint64_t fskit_entry_get_file_id( struct fskit_entry* ent );
void* fskit_entry_get_user_data( struct fskit_entry* ent );
uint8_t fskit_entry_get_type( struct fskit_entry* ent );
uint64_t fskit_entry_get_owner( struct fskit_entry* ent );
uint64_t fskit_entry_get_group( struct fskit_entry* ent );
mode_t fskit_entry_get_mode( struct fskit_entry* ent );
int32_t fskit_entry_get_link_count( struct fskit_entry* ent );
void fskit_entry_get_atime( struct fskit_entry* ent, int64_t* atime_sec, int32_t* atime_nsec );
void fskit_entry_get_mtime( struct fskit_entry* ent, int64_t* mtime_sec, int32_t* mtime_nsec );
void fskit_entry_get_ctime( struct fskit_entry* ent, int64_t* ctime_sec, int32_t* ctime_nsec );
off_t fskit_entry_get_size( struct fskit_entry* ent ); 
dev_t fskit_entry_get_rdev( struct fskit_entry* ent );
fskit_entry_set* fskit_entry_get_children( struct fskit_entry* ent );
fskit_xattr_set* fskit_entry_get_xattrs( struct fskit_entry* ent );
int64_t fskit_entry_get_num_children( struct fskit_entry* ent );

// file handle getters
char* fskit_file_handle_get_path( struct fskit_file_handle* fh );
struct fskit_entry* fskit_file_handle_get_entry( struct fskit_file_handle* fh );
void* fskit_file_handle_get_user_data( struct fskit_file_handle* fh );

// dir handle getters
char* fskit_dir_handle_get_path( struct fskit_dir_handle* fh );
struct fskit_entry* fskit_dir_handle_get_entry( struct fskit_dir_handle* fh );
void* fskit_dir_handle_get_user_data( struct fskit_dir_handle* fh );

// setters
int fskit_entry_set_user_data( struct fskit_entry* ent, void* app_data );
void fskit_entry_set_file_id( struct fskit_entry* ent, uint64_t file_id );
fskit_entry_set* fskit_entry_swap_children( struct fskit_entry* ent, fskit_entry_set* new_children );
fskit_xattr_set* fskit_entry_swap_xattrs( struct fskit_entry* ent, fskit_xattr_set* new_xattrs );
char* fskit_entry_swap_symlink_target( struct fskit_entry* ent, char* new_symlink_target );

FSKIT_C_LINKAGE_END 

#endif
