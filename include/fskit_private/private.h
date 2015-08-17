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

struct fskit_xattr_set_entry {
   
   char* name;
   char* value;
   size_t value_len;
   
   struct fskit_xattr_set_entry* left;
   struct fskit_xattr_set_entry* right;
   char color;
};

// xattrs
struct fskit_xattr_set_entry;
typedef struct fskit_xattr_set_entry fskit_xattr_set;

#define FSKIT_XATTR_SET_ENTRY_CMP( x1, x2 ) (strcmp((x1)->name, (x2)->name))

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

   bool deletion_in_progress;   // set to true if this node is flagged for garbage-collection

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
};

// fskit workqueue request
struct fskit_wreq {

   // callback to do work
   fskit_wq_func_t work;

   // user-supplied arguments
   void* work_data;

   // flags controlling the lifecycle of this work request
   int flags;

   // promise semaphore, to wake up the caller.
   // only initialized of FSKIT_WQ_PROMISE is specified
   sem_t promise_sem;
   int promise_ret;
   
   struct fskit_wreq* next;     // pointer to next work element
};

// fskit workqueue
struct fskit_wq {

   
   // worker thread
   pthread_t thread;

   // is the thread running?
   volatile bool running;

   // things to do
   struct fskit_wreq* work;
   struct fskit_wreq* tail;

   // lock governing access to work
   pthread_mutex_t work_lock;

   // semaphore to signal the availability of work
   sem_t work_sem;
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
   fskit_entry_route_rename_callback_t       rename_cb;
};

// metadata about the patch matched to the route
// TODO: union
struct fskit_route_metadata {
   char* path;                  // the path matched
   int argc;                    // number of matches
   char** argv;                 // each matched string in the path regex
   
   struct fskit_entry* parent;  // parent entry (creat(), mknod(), mkdir(), rename() only)
   
   struct fskit_entry* new_parent;      // parent entry of the destination (rename() only)
   char* new_path;                      // path to rename to (rename() only)
   
   bool garbage_collect;        // is this entry being unlinked due to garbage-collection, or due to an explicit command from userspace?
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

   struct stat* sb;      // stat() only
   
   struct fskit_entry* parent;  // create(), mkdir(), mknod(), rename() (guaranteed to be write-locked)
   
   struct fskit_entry* new_parent;      // rename() only (guaranteed to be write-locked)
   struct fskit_entry* dest;    // rename() only (not locked)
   char const* new_path;      // rename() only
   
   bool garbage_collect;        // is this entry being unlinked due to garbage-collection, or due to an explicit command from userspace?
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
int fskit_run_user_create( struct fskit_core* core, char const* path, struct fskit_entry* parent, struct fskit_entry* fent, mode_t mode, void** inode_data, void** handle_data );
int fskit_do_create( struct fskit_core* core, struct fskit_entry* parent, char const* path, mode_t mode, uint64_t user, uint64_t group, struct fskit_entry** ret_child, void** handle_data );

// private--needed by opendir()
int fskit_run_user_open( struct fskit_core* core, char const* path, struct fskit_entry* fent, int flags, void** handle_data );

// private--needed by read
ssize_t fskit_run_user_read( struct fskit_core* core, char const* path, struct fskit_entry* fent, char* buf, size_t buflen, off_t offset, void* handle_data );

// private--needed by any detach logic
int fskit_run_user_detach( struct fskit_core* core, char const* path, struct fskit_entry* fent );

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

// xattr 
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

// populate route dispatch arguments (internal API)
int fskit_route_create_args( struct fskit_route_dispatch_args* dargs, struct fskit_entry* parent, mode_t mode );
int fskit_route_mknod_args( struct fskit_route_dispatch_args* dargs, struct fskit_entry* parent, mode_t mode, dev_t dev );
int fskit_route_mkdir_args( struct fskit_route_dispatch_args* dargs, struct fskit_entry* parent, mode_t mode );
int fskit_route_open_args( struct fskit_route_dispatch_args* dargs, int flags );
int fskit_route_close_args( struct fskit_route_dispatch_args* dargs, void* handle_data );
int fskit_route_readdir_args( struct fskit_route_dispatch_args* dargs, struct fskit_dir_entry** dents, uint64_t num_dents );
int fskit_route_io_args( struct fskit_route_dispatch_args* dargs, char* iobuf, size_t iolen, off_t iooff, void* handle_data, fskit_route_io_continuation io_cont );
int fskit_route_trunc_args( struct fskit_route_dispatch_args* dargs, off_t iooff, void* handle_data, fskit_route_io_continuation io_cont );
int fskit_route_detach_args( struct fskit_route_dispatch_args* dargs, bool garbage_collect, void* inode_data );
int fskit_route_stat_args( struct fskit_route_dispatch_args* dargs, struct stat* sb );
int fskit_route_sync_args( struct fskit_route_dispatch_args* dargs );
int fskit_route_rename_args( struct fskit_route_dispatch_args* dargs, struct fskit_entry* old_parent, char const* new_path, struct fskit_entry* new_parent, struct fskit_entry* dest );

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
int fskit_route_call_stat( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_sync( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_rename( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );

// memory management (internal API)
int fskit_path_route_free( struct fskit_path_route* route );

// private--needed by open()
int fskit_run_user_trunc( struct fskit_core* core, char const* path, struct fskit_entry* fent, off_t new_size, void* handle_data );

#endif