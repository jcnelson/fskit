/*
   fskit: a library for creating multi-threaded in-RAM filesystems
   Copyright (C) 2015  Jude Nelson

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

#ifndef _FSKIT_PRIVATE_H_
#define _FSKIT_PRIVATE_H_

#include <fskit/common.h>
#include <fskit/debug.h>
#include <fskit/sglib.h>
#include <fskit/route.h>

struct fskit_route_table_row;
typedef struct fskit_route_table_row fskit_route_table;

// xattrs
struct fskit_xattr_set_entry;
typedef struct fskit_xattr_set_entry fskit_xattr_set;

struct fskit_xattr_set_entry {
   
   char* name;
   char* value;
   size_t value_len;
   
   struct fskit_xattr_set_entry* left;
   struct fskit_xattr_set_entry* right;
   char color;
};

// fskit inode structure
struct fskit_entry {
   uint64_t file_id;             // inode number
   uint8_t type;                 // type of inode

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

   bool deletion_in_progress;   // set to true if this node is flagged for garbage-collection.  valid only for directories

   // if this is a directory, this is allocated and points to a fskit_entry_set
   int64_t num_children;
   fskit_entry_set* children;

   // application-defined entry data
   void* app_data;

   // if this is a special file, this is the device major/minor number
   dev_t dev;

   // lock governing access to the above structure fields
   pthread_rwlock_t lock;

   // extended attributes
   fskit_xattr_set* xattrs;
   
   // if this is a symlink, this is the target
   char* symlink_target;
};

// file handle structure
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

// directory handle structure
struct fskit_telldir_entry;

struct fskit_dir_handle {

   struct fskit_entry* dent;

   char* path;
   uint64_t file_id;
   
   // for iteration
   char curr_name[ FSKIT_FILESYSTEM_NAMEMAX+1 ];
   
   // for seekdir/telldir 
   struct fskit_telldir_entry* telldir_list;

   // lock governing access to this structure
   pthread_rwlock_t lock;

   // application-defined data
   void* app_data;
   
   // eof?
   bool eof;
};

// fskit core filesystem structure
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
   fskit_route_table* routes;

   // lock governing access to the above fields of this structure
   pthread_rwlock_t route_lock;

   // extra features to enable 
   uint64_t features;
};

// route method type 
union fskit_route_method {
   fskit_entry_route_create_callback_t       create_cb;
   fskit_entry_route_mknod_callback_t        mknod_cb;
   fskit_entry_route_mkdir_callback_t        mkdir_cb;
   fskit_entry_route_open_callback_t         open_cb;
   fskit_entry_route_close_callback_t        close_cb;
   fskit_entry_route_io_callback_t           io_cb;
   fskit_entry_route_trunc_callback_t        trunc_cb;
   fskit_entry_route_sync_callback_t         sync_cb;
   fskit_entry_route_stat_callback_t         stat_cb;
   fskit_entry_route_readdir_callback_t      readdir_cb;
   fskit_entry_route_detach_callback_t       detach_cb;
   fskit_entry_route_destroy_callback_t      destroy_cb;
   fskit_entry_route_rename_callback_t       rename_cb;
   fskit_entry_route_link_callback_t         link_cb;
   fskit_entry_route_getxattr_callback_t     getxattr_cb;
   fskit_entry_route_setxattr_callback_t     setxattr_cb;
   fskit_entry_route_listxattr_callback_t    listxattr_cb;
   fskit_entry_route_removexattr_callback_t  removexattr_cb;
};

// metadata about the patch matched to the route
// TODO: union
struct fskit_route_metadata {
   char* path;                  // the path matched
   int argc;                    // number of matches
   char** argv;                 // each matched string in the path regex
   
   struct fskit_entry* parent;  // parent entry (creat(), mknod(), mkdir(), rename() only)
   char* name;
   
   struct fskit_entry* new_parent;      // parent entry of the destination (rename(), link())
   char* new_path;                      // path to rename/link to (rename(), link())
   
   bool garbage_collect;        // is this entry being unlinked due to garbage-collection, or due to an explicit command from userspace?
   void* cls;                   // user-given argument to the method at hand

   char const* xattr_name;
   char const* xattr_value;
   size_t xattr_value_len;
   char* xattr_buf;
   size_t xattr_buf_len;
};

// route dispatch arguments
// TODO: union
struct fskit_route_dispatch_args {

   int flags;           // open() only

   mode_t mode;         // create(), mknod() only
   dev_t dev;           // mknod() only

   void* inode_data;    // create(), mkdir(), unlink(), rmdir() only.  In create() and mkdir(), this is an output value.
   void* handle_data;   // create(), open(), opendir(), close() only.  In open() and opendir(), this is an output value.

   char* iobuf;         // read(), write() only.  In read(), this is an output value.
   size_t iolen;        // read(), write() only
   off_t iooff;         // read(), write(), trunc() only
   fskit_route_io_continuation io_cont;  // read(), write(), trunc() only

   struct fskit_dir_entry** dents;        // readdir() only
   uint64_t num_dents;

   char const* name;
   struct stat* sb;      // stat() only
   bool fent_absent;     // stat() only
   
   struct fskit_entry* parent;  // create(), mkdir(), mknod(), rename(), link(), rmdir(), unlink() (guaranteed to be write-locked if non-NULL)
   
   struct fskit_entry* new_parent;      // rename(), link() (guaranteed to be write-locked)
   struct fskit_entry* dest;    // rename() only (not locked)
   char const* new_path;      // rename(), link()
   
   bool garbage_collect;        // is this entry being unlinked due to garbage-collection, or due to an explicit command from userspace?

   // for xattrs 
   char const* xattr_name;
   char const* xattr_value;
   size_t xattr_value_len;
   char* xattr_buf;
   size_t xattr_buf_len;
   int xattr_flags;

   void* cls;               // create(), mknod(), mkdir(), only
};

// a path route
struct fskit_path_route {

   char* path_regex_str;                // string-ified regex
   int num_expected_matches;            // number of expected match groups (upper bound)
   regex_t path_regex;                  // compiled regular expression

   int consistency_discipline;          // concurrent or sequential call?

   int route_type;                      // one of FSKIT_ROUTE_MATCH_*
   union fskit_route_method method;           // which method to call

   pthread_rwlock_t lock;               // lock used to enforce the consistency discipline
};

// garbage collection 
int fskit_entry_try_garbage_collect( struct fskit_core* core, char const* path, struct fskit_entry* parent, struct fskit_entry* child );

// private--needed by closedir()
int fskit_run_user_close( struct fskit_core* core, char const* path, struct fskit_entry* fent, void* handle_data );

// private--needed by open()
int fskit_run_user_create( struct fskit_core* core, char const* path, struct fskit_entry* parent, struct fskit_entry* fent, mode_t mode, void* cls, void** inode_data, void** handle_data );
int fskit_do_create( struct fskit_core* core, struct fskit_entry* parent, char const* path, mode_t mode, uint64_t user, uint64_t group, void* cls, struct fskit_entry** ret_child, void** handle_data );
struct fskit_file_handle* fskit_open_ex( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, int flags, mode_t mode, void* cls, int* err );

// private--needed by opendir()
int fskit_run_user_open( struct fskit_core* core, char const* path, struct fskit_entry* fent, int flags, void** handle_data );

// private--needed by read
ssize_t fskit_run_user_read( struct fskit_core* core, char const* path, struct fskit_entry* fent, char* buf, size_t buflen, off_t offset, void* handle_data );

// private--needed by any detach logic
int fskit_run_user_detach( struct fskit_core* core, char const* path, struct fskit_entry* parent, struct fskit_entry* fent );

// routes 
typedef struct fskit_path_route* fskit_path_route_entry;
SGLIB_DEFINE_VECTOR_PROTOTYPES( fskit_path_route_entry );
typedef struct sglib_fskit_path_route_entry_vector fskit_route_list_t;

// route tables
#define FSKIT_ROUTE_TABLE_ROW_CMP( r1, r2 ) ((r1)->route_type - (r2)->route_type)

SGLIB_DEFINE_RBTREE_PROTOTYPES( fskit_route_table, left, right, color, FSKIT_ROUTE_TABLE_ROW_CMP );
typedef struct sglib_fskit_route_table_iterator fskit_route_table_itr;

fskit_route_table* fskit_route_table_new(void);
int fskit_route_table_free( fskit_route_table* routes );
int fskit_route_table_row_free( struct fskit_route_table_row* row );
int fskit_route_table_insert( fskit_route_table** routes, int route_type, struct fskit_path_route* route );
struct fskit_route_table_row* fskit_route_table_get_row( fskit_route_table* routes, int route_type );
struct fskit_path_route* fskit_route_table_find( fskit_route_table* routes, int route_type, int route_id );
struct fskit_path_route* fskit_route_table_remove( fskit_route_table** route_table, int route_type, int route_id );

// populate route dispatch arguments (internal API)
int fskit_route_create_args( struct fskit_route_dispatch_args* dargs, struct fskit_entry* parent, char const* name, mode_t mode, void* cls );
int fskit_route_mknod_args( struct fskit_route_dispatch_args* dargs, struct fskit_entry* parent, char const* name, mode_t mode, dev_t dev, void* cls );
int fskit_route_mkdir_args( struct fskit_route_dispatch_args* dargs, struct fskit_entry* parent, char const* name, mode_t mode, void* cls );
int fskit_route_open_args( struct fskit_route_dispatch_args* dargs, char const* name, int flags );
int fskit_route_close_args( struct fskit_route_dispatch_args* dargs, void* handle_data );
int fskit_route_readdir_args( struct fskit_route_dispatch_args* dargs, char const* name, struct fskit_dir_entry** dents, uint64_t num_dents );
int fskit_route_io_args( struct fskit_route_dispatch_args* dargs, char* iobuf, size_t iolen, off_t iooff, void* handle_data, fskit_route_io_continuation io_cont );
int fskit_route_trunc_args( struct fskit_route_dispatch_args* dargs, char const* name, off_t iooff, void* handle_data, fskit_route_io_continuation io_cont );
int fskit_route_detach_args( struct fskit_route_dispatch_args* dargs, struct fskit_entry* parent, char const* name, bool garbage_collect, void* inode_data );
int fskit_route_destroy_args( struct fskit_route_dispatch_args* dargs, struct fskit_entry* parent, char const* name, void* inode_data );
int fskit_route_stat_args( struct fskit_route_dispatch_args* dargs, char const* name, struct stat* sb, bool fent_absent );
int fskit_route_sync_args( struct fskit_route_dispatch_args* dargs );
int fskit_route_rename_args( struct fskit_route_dispatch_args* dargs, struct fskit_entry* old_parent, char const* old_name, char const* new_path, struct fskit_entry* new_parent, struct fskit_entry* dest );
int fskit_route_link_args( struct fskit_route_dispatch_args* dargs, char const* name, char const* new_path, struct fskit_entry* new_parent );
int fskit_route_getxattr_args( struct fskit_route_dispatch_args* args, char const* xattr_name, char* xattr_buf, size_t xattr_buf_len );
int fskit_route_setxattr_args( struct fskit_route_dispatch_args* args, char const* xattr_name, char const* xattr_value, size_t xattr_value_len, int flags );
int fskit_route_listxattr_args( struct fskit_route_dispatch_args* args, char* xattr_buf, size_t xattr_buf_len );
int fskit_route_removexattr_args( struct fskit_route_dispatch_args* args, char const* xattr_name );

// call user-supplied routes (internal API)
int fskit_route_call_create( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_mknod( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_mkdir( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_open( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_close( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_readdir( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_read( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_write( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_trunc( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_detach( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_destroy( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_stat( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_sync( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_rename( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_link( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_getxattr( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_listxattr( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_setxattr( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_removexattr( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );

// memory management (internal API)
int fskit_path_route_free( struct fskit_path_route* route );

// private--needed by open()
int fskit_run_user_trunc( struct fskit_core* core, char const* path, struct fskit_entry* fent, off_t new_size, void* handle_data );

#endif
