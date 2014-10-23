/*
   fskit: a library for creating multi-threaded in-RAM filesystems
   Copyright (C) 2014  Jude Nelson

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _FSKIT_FUSE_H_
#define _FSKIT_FUSE_H_

#include "fskit.h"

#define FUSE_USE_VERSION 28
#include <fuse.h>

#include <map>
#include <string>
#include <set>

using namespace std;

// private fuse state
struct fskit_fuse_state {
   
   struct fskit_core* core;
};

// fskit fuse file handle
struct fskit_fuse_file_info {
   
   int type;
   union {
      struct fskit_file_handle* fh;
      struct fskit_dir_handle* dh;
   } handle;
};

extern "C" {
   
// access to state
struct fskit_fuse_state* fskit_fuse_get_state();
   
// fs methods
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
int fskit_fuse_main( struct fskit_fuse_state* state, int argc, char** argv );
int fskit_fuse_shutdown( struct fskit_fuse_state* state, void** user_state );
   
}

#endif
