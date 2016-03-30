/*
   fuse-demo: a FUSE filesystem demo of fskit
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

#ifndef _FSKIT_FUSE_H_
#define _FSKIT_FUSE_H_


#define _DEFAULT_SOURCE

#include <fskit/fskit.h>

#define FUSE_USE_VERSION 28

#include <fuse.h>

// allow the filesystem process to call arbitrary methods on itself externally, bypassing permissions checks
#define FSKIT_FUSE_SET_FS_ACCESS        0x1

// disable uid/gid checking; every call is from "root"
#define FSKIT_FUSE_NO_PERMISSIONS       0x2

// call route on stat even if the inode doesn't exist 
#define FSKIT_FUSE_STAT_ON_ABSENT       0x4

FSKIT_C_LINKAGE_BEGIN

struct fskit_fuse_state;
typedef int (*fskit_fuse_postmount_callback_t)( struct fskit_fuse_state*, void* );

// fskit fuse file handle
struct fskit_fuse_file_info {

   int type;
   union {
      struct fskit_file_handle* fh;
      struct fskit_dir_handle* dh;
   } handle;
};

// access to state
struct fskit_fuse_state* fskit_fuse_state_new();
void fskit_fuse_state_free( struct fskit_fuse_state* );
struct fskit_fuse_state* fskit_fuse_get_state();
uid_t fskit_fuse_get_uid( struct fskit_fuse_state* state );
gid_t fskit_fuse_get_gid( struct fskit_fuse_state* state );
pid_t fskit_fuse_get_pid();
mode_t fskit_fuse_get_umask();

int fskit_fuse_setting_enable( struct fskit_fuse_state* state, uint64_t flag );
int fskit_fuse_setting_disable( struct fskit_fuse_state* state, uint64_t flag );

char const* fskit_fuse_get_mountpoint( struct fskit_fuse_state* state );
int fskit_fuse_postmount_callback( struct fskit_fuse_state* state, fskit_fuse_postmount_callback_t cb, void* cb_cls );

// default fs methods
int fuse_fskit_getattr(const char *path, struct stat *statbuf);
int fuse_fskit_readlink(const char *path, char *link, size_t size);
int fuse_fskit_mknod(const char *path, mode_t mode, dev_t dev);
int fuse_fskit_mkdir(const char *path, mode_t mode);
int fuse_fskit_unlink(const char *path);
int fuse_fskit_rmdir(const char *path);
int fuse_fskit_symlink(const char *path, const char *link);
int fuse_fskit_rename(const char *path, const char *newpath);
int fuse_fskit_link(const char *path, const char *newpath);
int fuse_fskit_chmod(const char *path, mode_t mode);
int fuse_fskit_chown(const char *path, uid_t uid, gid_t gid);
int fuse_fskit_truncate(const char *path, off_t newsize);
int fuse_fskit_utime(const char *path, struct utimbuf *ubuf);
int fuse_fskit_open(const char *path, struct fuse_file_info *fi);
int fuse_fskit_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int fuse_fskit_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int fuse_fskit_statfs(const char *path, struct statvfs *statv);
int fuse_fskit_flush(const char *path, struct fuse_file_info *fi);
int fuse_fskit_release(const char *path, struct fuse_file_info *fi);
int fuse_fskit_fsync(const char *path, int datasync, struct fuse_file_info *fi);
int fuse_fskit_setxattr(const char *path, const char *name, const char *value, size_t size, int flags);
int fuse_fskit_getxattr(const char *path, const char *name, char *value, size_t size);
int fuse_fskit_listxattr(const char *path, char *list, size_t size);
int fuse_fskit_removexattr(const char *path, const char *name);
int fuse_fskit_opendir(const char *path, struct fuse_file_info *fi);
int fuse_fskit_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
int fuse_fskit_releasedir(const char *path, struct fuse_file_info *fi);
int fuse_fskit_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi);
int fuse_fskit_access(const char *path, int mask);
int fuse_fskit_create(const char *path, mode_t mode, struct fuse_file_info *fi);
int fuse_fskit_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi);
int fuse_fskit_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi);
void *fuse_fskit_fuse_init(struct fuse_conn_info *conn);
void fuse_fskit_destroy(void *userdata);

// get all fs methods
struct fuse_operations fskit_fuse_get_opers();

// main interface
int fskit_fuse_init( struct fskit_fuse_state* state, void* user_state );
int fskit_fuse_init_fs( struct fskit_fuse_state* state, struct fskit_core* fs );
int fskit_fuse_main( struct fskit_fuse_state* state, int argc, char** argv );
int fskit_fuse_shutdown( struct fskit_fuse_state* state, void** user_state );

struct fskit_core* fskit_fuse_get_core( struct fskit_fuse_state* state );
void fskit_fuse_detach_core( struct fskit_fuse_state* state );

FSKIT_C_LINKAGE_END 

#endif
