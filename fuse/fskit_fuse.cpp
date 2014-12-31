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

#include "fskit_fuse.h"

// get running state
struct fskit_fuse_state* fskit_fuse_get_state() {
   return (struct fskit_fuse_state*)fuse_get_context()->private_data;
}

// get caller UID
uid_t fskit_fuse_get_uid( struct fskit_fuse_state* state ) {

   if( getpid() == fuse_get_context()->pid && (state->settings & FSKIT_FUSE_SET_FS_ACCESS) ) {
      // filesystem process can access anything
      return 0;
   }
   else {
      return fuse_get_context()->uid;
   }
}

// get caller GID
gid_t fskit_fuse_get_gid( struct fskit_fuse_state* state ) {

   if( getpid() == fuse_get_context()->pid && (state->settings & FSKIT_FUSE_SET_FS_ACCESS) ) {
      // filesystem process can access anything
      return 0;
   }
   else {
      return fuse_get_context()->gid;
   }
}

// get caller PID
pid_t fskit_fuse_get_pid() {
   return fuse_get_context()->pid;
}

// get caller umask
mode_t fskit_fuse_get_umask() {
   return fuse_get_context()->umask;
}

// enable a setting
int fskit_fuse_setting_enable( struct fskit_fuse_state* state, uint64_t flag ) {
   state->settings |= flag;
   return 0;
}

// disable a setting
int fskit_fuse_setting_disable( struct fskit_fuse_state* state, uint64_t flag ) {
   state->settings &= ~flag;
   return 0;
}

// set the postmount callback
int fskit_fuse_postmount_callback( struct fskit_fuse_state* state, fskit_fuse_postmount_callback_t cb, void* cb_cls ) {
   state->postmount = cb;
   state->postmount_cls = cb_cls;
   return 0;
}

// make a FUSE file info for a file handle
struct fskit_fuse_file_info* fskit_fuse_make_file_handle( struct fskit_file_handle* fh ) {

   struct fskit_fuse_file_info* ffi = (struct fskit_fuse_file_info*)calloc( sizeof(struct fskit_fuse_file_info), 1 );
   if( ffi == NULL ) {
      return NULL;
   }

   ffi->type = FSKIT_ENTRY_TYPE_FILE;
   ffi->handle.fh = fh;

   return ffi;
}

// make a FUSE file info for a dir handle
struct fskit_fuse_file_info* fskit_fuse_make_dir_handle( struct fskit_dir_handle* dh ) {

   struct fskit_fuse_file_info* ffi = (struct fskit_fuse_file_info*)calloc( sizeof(struct fskit_fuse_file_info), 1 );
   if( ffi == NULL ) {
      return NULL;
   }

   ffi->type = FSKIT_ENTRY_TYPE_DIR;
   ffi->handle.dh = dh;

   return ffi;
}

int fskit_fuse_getattr(const char *path, struct stat *statbuf) {

   fskit_debug("getattr(%s, %p)\n", path, statbuf );

   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uid_t uid = fskit_fuse_get_uid( state );
   gid_t gid = fskit_fuse_get_gid( state );

   int rc = fskit_stat( state->core, path, uid, gid, statbuf );

   fskit_debug("getattr(%s, %p) rc = %d\n", path, statbuf, rc );

   return rc;
}


int fskit_fuse_readlink(const char *path, char *link, size_t size) {

   fskit_debug("readlink(%s, %zu)\n", path, size );

   ssize_t rc = 0;
   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uid_t uid = fskit_fuse_get_uid( state );
   gid_t gid = fskit_fuse_get_gid( state );

   rc = fskit_readlink( state->core, path, uid, gid, link, size );

   if( rc >= 0 ) {
      rc = 0;
   }

   fskit_debug("readlink(%s, %zu) rc = %zd\n", path, size, rc );

   return (int)rc;
}


int fskit_fuse_mknod(const char *path, mode_t mode, dev_t dev) {

   fskit_debug("mknod(%s, %o, %d, %d)\n", path, mode, major(dev), minor(dev) );

   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uid_t uid = fskit_fuse_get_uid( state );
   gid_t gid = fskit_fuse_get_gid( state );

   int rc = fskit_mknod( state->core, path, mode, dev, uid, gid );

   fskit_debug("mknod(%s, %o, %d, %d) rc = %d\n", path, mode, major(dev), minor(dev), rc );

   return rc;
}

int fskit_fuse_mkdir(const char *path, mode_t mode) {

   fskit_debug("mkdir(%s, %o)\n", path, mode );

   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uid_t uid = fskit_fuse_get_uid( state );
   gid_t gid = fskit_fuse_get_gid( state );

   int rc = fskit_mkdir( state->core, path, mode, uid, gid );

   fskit_debug("mkdir(%s, %o) rc = %d\n", path, mode, rc );

   return rc;
}

int fskit_fuse_unlink(const char *path) {

   fskit_debug("unlink(%s)\n", path );

   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uid_t uid = fskit_fuse_get_uid( state );
   gid_t gid = fskit_fuse_get_gid( state );

   int rc = fskit_unlink( state->core, path, uid, gid );

   fskit_debug("unlink(%s) rc = %d\n", path, rc );

   return rc;
}

int fskit_fuse_rmdir(const char *path) {

   fskit_debug("rmdir(%s)\n", path );

   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uid_t uid = fskit_fuse_get_uid( state );
   gid_t gid = fskit_fuse_get_gid( state );

   int rc = fskit_rmdir( state->core, path, uid, gid );

   fskit_debug("rmdir(%s) rc = %d\n", path, rc );

   return rc;
}

int fskit_fuse_symlink(const char *target, const char *linkpath) {

   fskit_debug("symlink(%s, %s)\n", target, linkpath );

   int rc = 0;
   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uid_t uid = fskit_fuse_get_uid( state );
   gid_t gid = fskit_fuse_get_gid( state );

   rc = fskit_symlink( state->core, target, linkpath, uid, gid );

   fskit_debug("symlink(%s, %s) rc = %d\n", target, linkpath, rc );
   return rc;
}

int fskit_fuse_rename(const char *path, const char *newpath) {

   fskit_debug("rename(%s, %s)\n", path, newpath );

   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uid_t uid = fskit_fuse_get_uid( state );
   gid_t gid = fskit_fuse_get_gid( state );

   int rc = fskit_rename( state->core, path, newpath, uid, gid );

   fskit_debug("rename(%s, %s) rc = %d\n", path, newpath, rc );

   return rc;
}

int fskit_fuse_link(const char *path, const char *newpath) {

   fskit_debug("link(%s, %s)\n", path, newpath );
   // not supported by fskit
   fskit_debug("link(%s, %s) rc = %d\n", path, newpath, -ENOSYS );

   return -ENOSYS;
}

int fskit_fuse_chmod(const char *path, mode_t mode) {

   fskit_debug("chmod(%s, %o)\n", path, mode );

   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uid_t uid = fskit_fuse_get_uid( state );
   gid_t gid = fskit_fuse_get_gid( state );

   int rc = fskit_chmod( state->core, path, uid, gid, mode );

   fskit_debug("chmod(%s, %o) rc = %d\n", path, mode, rc );

   return rc;
}

int fskit_fuse_chown(const char *path, uid_t new_uid, gid_t new_gid) {

   fskit_debug("chown(%s, %d, %d)\n", path, new_uid, new_gid );

   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uid_t uid = fskit_fuse_get_uid( state );
   gid_t gid = fskit_fuse_get_gid( state );

   int rc = fskit_chown( state->core, path, uid, gid, new_uid, new_gid );

   fskit_debug("chown(%s, %d, %d) rc = %d\n", path, new_uid, new_gid, rc );

   return rc;
}

int fskit_fuse_truncate(const char *path, off_t newsize) {

   fskit_debug("truncate(%s, %jd)\n", path, newsize );

   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uid_t uid = fskit_fuse_get_uid( state );
   gid_t gid = fskit_fuse_get_gid( state );

   int rc = fskit_trunc( state->core, path, uid, gid, newsize );

   fskit_debug("truncate(%s, %jd) rc = %d\n", path, newsize, rc );

   return rc;
}

int fskit_fuse_utime(const char *path, struct utimbuf *ubuf) {

   fskit_debug("utime(%s, %ld.%ld)\n", path, ubuf->actime, ubuf->modtime );

   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uid_t uid = fskit_fuse_get_uid( state );
   gid_t gid = fskit_fuse_get_gid( state );

   int rc = fskit_utime( state->core, path, uid, gid, ubuf );

   fskit_debug("utime(%s, %ld.%ld) rc = %d\n", path, ubuf->actime, ubuf->modtime, rc );

   return rc;
}

int fskit_fuse_open(const char *path, struct fuse_file_info *fi) {

   fskit_debug("open(%s, %p)\n", path, fi);

   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uid_t uid = fskit_fuse_get_uid( state );
   gid_t gid = fskit_fuse_get_gid( state );
   mode_t umask = fskit_fuse_get_umask();
   struct fskit_fuse_file_info* ffi = NULL;
   int rc = 0;

   struct fskit_file_handle* fh = fskit_open( state->core, path, uid, gid, fi->flags, ~umask, &rc );

   if( rc != 0 ) {

      fskit_debug("open(%s, %p) rc = %d\n", path, fi, rc);
      return rc;
   }

   ffi = fskit_fuse_make_file_handle( fh );
   if( ffi == NULL ) {

      fskit_close( state->core, fh );
      return -ENOMEM;
   }

   fi->fh = (uint64_t)ffi;

   // NOTE: fskit_read() and fskit_write() return a negative error code on error,
   // so set direct_io to allow this error code to be propagated.
   fi->direct_io = 1;

   fskit_debug("open(%s, %p) rc = %d\n", path, fi, rc);

   return rc;
}

int fskit_fuse_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {

   fskit_debug("read(%s, %p, %zu, %jd, %p)\n", path, buf, size, offset, fi);

   struct fskit_fuse_state* state = fskit_fuse_get_state();

   struct fskit_fuse_file_info* ffi = (struct fskit_fuse_file_info*)fi->fh;
   ssize_t num_read = 0;

   num_read = fskit_read( state->core, ffi->handle.fh, buf, size, offset );

   fskit_debug("read(%s, %p, %zu, %jd, %p) rc = %zd\n", path, buf, size, offset, fi, num_read);

   return (int)num_read;
}

int fskit_fuse_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {

   fskit_debug("write(%s, %p, %zu, %jd, %p)\n", path, buf, size, offset, fi);

   struct fskit_fuse_state* state = fskit_fuse_get_state();

   struct fskit_fuse_file_info* ffi = (struct fskit_fuse_file_info*)fi->fh;
   ssize_t num_written = 0;

   num_written = fskit_write( state->core, ffi->handle.fh, buf, size, offset );

   fskit_debug("write(%s, %p, %zu, %jd, %p) rc = %zd\n", path, buf, size, offset, fi, num_written);

   return (int)num_written;
}

int fskit_fuse_statfs(const char *path, struct statvfs *statv) {

   fskit_debug("statfs(%s, %p)\n", path, statv );

   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uid_t uid = fskit_fuse_get_uid( state );
   gid_t gid = fskit_fuse_get_gid( state );

   int rc = fskit_statvfs( state->core, path, uid, gid, statv );

   fskit_debug("statfs(%s, %p) rc = %d\n", path, statv, rc );

   return rc;
}

int fskit_fuse_flush(const char *path, struct fuse_file_info *fi) {

   fskit_debug("flush(%s, %p)\n", path, fi);

   // not addressed by fskit

   fskit_debug("flush(%s, %p) rc = %d\n", path, fi, 0);
   return 0;
}

int fskit_fuse_release(const char *path, struct fuse_file_info *fi) {

   fskit_debug("release(%s, %p)\n", path, fi);

   struct fskit_fuse_state* state = fskit_fuse_get_state();

   struct fskit_fuse_file_info* ffi = (struct fskit_fuse_file_info*)fi->fh;

   int rc = fskit_close( state->core, ffi->handle.fh );

   if( rc == 0 ) {
      free( ffi );
   }

   fskit_debug("release(%s, %p) rc = %d\n", path, fi, rc);
   return rc;
}

int fskit_fuse_fsync(const char *path, int datasync, struct fuse_file_info *fi) {

   fskit_debug("fsync(%s, %d, %p)\n", path, datasync, fi );

   // not addressed by fskit

   fskit_debug("fsync(%s, %d, %p) rc = %d\n", path, datasync, fi, 0 );
   return 0;
}

int fskit_fuse_setxattr(const char *path, const char *name, const char *value, size_t size, int flags) {

   fskit_debug("setxattr(%s, %s, %p, %zu, %X)\n", path, name, value, size, flags );

   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uid_t uid = fskit_fuse_get_uid( state );
   gid_t gid = fskit_fuse_get_gid( state );

   int rc = fskit_setxattr( state->core, path, uid, gid, name, value, size, flags );

   fskit_debug("setxattr(%s, %s, %p, %zu, %X) rc = %d\n", path, name, value, size, flags, rc );
   return rc;
}

int fskit_fuse_getxattr(const char *path, const char *name, char *value, size_t size) {

   fskit_debug("getxattr(%s, %s, %p, %zu)\n", path, name, value, size );

   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uid_t uid = fskit_fuse_get_uid( state );
   gid_t gid = fskit_fuse_get_gid( state );

   int rc = fskit_getxattr( state->core, path, uid, gid, name, value, size );

   fskit_debug("getxattr(%s, %s, %p, %zu) rc = %d\n", path, name, value, size, rc );
   return rc;
}

int fskit_fuse_listxattr(const char *path, char *list, size_t size) {

   fskit_debug("listxattr(%s, %p, %zu)\n", path, list, size );

   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uid_t uid = fskit_fuse_get_uid( state );
   gid_t gid = fskit_fuse_get_gid( state );

   int rc = fskit_listxattr( state->core, path, uid, gid, list, size );

   fskit_debug("listxattr(%s, %p, %zu) rc = %d\n", path, list, size, rc );
   return rc;
}

int fskit_fuse_removexattr(const char *path, const char *name) {

   fskit_debug("removexattr(%s, %s)\n", path, name );

   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uid_t uid = fskit_fuse_get_uid( state );
   gid_t gid = fskit_fuse_get_gid( state );

   int rc = fskit_removexattr( state->core, path, uid, gid, name );

   fskit_debug("removexattr(%s, %s) rc = %d\n", path, name, rc );
   return rc;
}

int fskit_fuse_opendir(const char *path, struct fuse_file_info *fi) {

   fskit_debug("opendir(%s, %p)\n", path, fi );

   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uid_t uid = fskit_fuse_get_uid( state );
   gid_t gid = fskit_fuse_get_gid( state );
   struct fskit_fuse_file_info* ffi = NULL;
   int rc = 0;

   struct fskit_dir_handle* dh = fskit_opendir( state->core, path, uid, gid, &rc );

   if( rc != 0 ) {

      fskit_debug("opendir(%s, %p) rc = %d\n", path, fi, rc );
      return rc;
   }

   ffi = fskit_fuse_make_dir_handle( dh );

   if( ffi == NULL ) {
      fskit_closedir( state->core, dh );

      fskit_debug("opendir(%s, %p) rc = %d\n", path, fi, -ENOMEM );
      return -ENOMEM;
   }

   fi->fh = (uint64_t)ffi;

   fskit_debug("opendir(%s, %p) rc = %d\n", path, fi, 0 );
   return 0;
}

int fskit_fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

   fskit_debug("readdir(%s, %jd, %p, %p)\n", path, offset, buf, fi );

   struct fskit_fuse_state* state = fskit_fuse_get_state();
   struct fskit_dir_handle* fdh = NULL;
   struct fskit_fuse_file_info* ffi = NULL;
   int rc = 0;
   uint64_t num_read = 0;

   ffi = (struct fskit_fuse_file_info*)fi->fh;
   fdh = ffi->handle.dh;

   struct fskit_dir_entry** dirents = fskit_listdir( state->core, fdh, &num_read, &rc );

   if( dirents == NULL || rc != 0 ) {
      fskit_debug("readdir(%s, %jd, %p, %p) rc = %d\n", path, offset, buf, fi, rc );
      return rc;
   }

   for( uint64_t i = 0; i < num_read; i++ ) {

      rc = filler( buf, dirents[i]->name, NULL, 0 );
      if( rc != 0 ) {
         rc = -ENOMEM;
         break;
      }
   }

   fskit_dir_entry_free_list( dirents );

   if( rc > 0 ) {
      rc = 0;
   }

   fskit_debug("readdir(%s, %jd, %p, %p) rc = %d\n", path, offset, buf, fi, rc );
   return rc;
}

int fskit_fuse_releasedir(const char *path, struct fuse_file_info *fi) {

   fskit_debug("releasedir(%s, %p)\n", path, fi );

   struct fskit_fuse_state* state = fskit_fuse_get_state();
   struct fskit_fuse_file_info* ffi = NULL;
   int rc = 0;

   ffi = (struct fskit_fuse_file_info*)fi->fh;

   rc = fskit_closedir( state->core, ffi->handle.dh );

   if( rc == 0 ) {
      free( ffi );
   }

   fskit_debug("releasedir(%s, %p) rc = %d\n", path, fi, rc );

   return rc;
}

int fskit_fuse_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi) {

   fskit_debug("fsyncdir(%s, %d, %p)\n", path, datasync, fi);

   // not addressed by fskit

   fskit_debug("fsyncdir(%s, %d, %p) rc = %d\n", path, datasync, fi, 0);
   return 0;
}

int fskit_fuse_access(const char *path, int mask) {

   fskit_debug("access(%s, %X)\n", path, mask );

   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uid_t uid = fskit_fuse_get_uid( state );
   gid_t gid = fskit_fuse_get_gid( state );

   int rc = fskit_access( state->core, path, uid, gid, mask );

   fskit_debug("access(%s, %X) rc = %d\n", path, mask, rc );

   return rc;
}

int fskit_fuse_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

   fskit_debug("create(%s, %o, %p)\n", path, mode, fi );

   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uid_t uid = fskit_fuse_get_uid( state );
   gid_t gid = fskit_fuse_get_gid( state );
   struct fskit_fuse_file_info* ffi = NULL;
   int rc = 0;

   struct fskit_file_handle* fh = fskit_create( state->core, path, uid, gid, mode, &rc );

   if( rc != 0 ) {

      fskit_debug("create(%s, %o, %p) rc = %d\n", path, mode, fi, rc );

      return rc;
   }

   ffi = fskit_fuse_make_file_handle( fh );
   if( ffi == NULL ) {

      fskit_close( state->core, fh );

      fskit_debug("create(%s, %o, %p) rc = %d\n", path, mode, fi, -ENOMEM );

      return -ENOMEM;
   }

   fi->fh = (uint64_t)ffi;

   // NOTE: fskit_read() and fskit_write() return a negative error code on error,
   // so set direct_io to allow this error code to be propagated.
   fi->direct_io = 1;

   fskit_debug("create(%s, %o, %p) rc = %d\n", path, mode, fi, rc );

   return rc;
}

int fskit_fuse_ftruncate(const char *path, off_t new_size, struct fuse_file_info *fi) {

   fskit_debug("ftruncate(%s, %zu, %p)\n", path, new_size, fi );

   struct fskit_fuse_state* state = fskit_fuse_get_state();
   struct fskit_fuse_file_info* ffi = NULL;

   ffi = (struct fskit_fuse_file_info*)fi->fh;

   int rc = fskit_ftrunc( state->core, ffi->handle.fh, new_size );

   fskit_debug("ftruncate(%s, %zu, %p) rc = %d\n", path, new_size, fi, rc );

   return rc;
}

int fskit_fuse_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi) {

   fskit_debug("fgetattr(%s, %p, %p)\n", path, statbuf, fi );

   struct fskit_fuse_state* state = fskit_fuse_get_state();
   struct fskit_fuse_file_info* ffi = NULL;

   ffi = (struct fskit_fuse_file_info*)fi->fh;

   int rc = 0;

   if( ffi->type == FSKIT_ENTRY_TYPE_FILE ) {
      rc = fskit_fstat( state->core, ffi->handle.fh->path, ffi->handle.fh->fent, statbuf );
   }
   else {
      rc = fskit_fstat( state->core, ffi->handle.dh->path, ffi->handle.dh->dent, statbuf );
   }

   fskit_debug("fgetattr(%s, %p, %p) rc = %d\n", path, statbuf, fi, rc );

   return rc;
}

void *fskit_fuse_fuse_init(struct fuse_conn_info *conn) {
   return fskit_fuse_get_state();
}

void fskit_fuse_destroy(void *userdata) {
   return;
}


// return the set of fuse operations
struct fuse_operations fskit_fuse_get_opers() {
   struct fuse_operations fo;
   memset(&fo, 0, sizeof(fo));

   fo.getattr = fskit_fuse_getattr;
   fo.readlink = fskit_fuse_readlink;
   fo.mknod = fskit_fuse_mknod;
   fo.mkdir = fskit_fuse_mkdir;
   fo.unlink = fskit_fuse_unlink;
   fo.rmdir = fskit_fuse_rmdir;
   fo.symlink = fskit_fuse_symlink;
   fo.rename = fskit_fuse_rename;
   fo.link = fskit_fuse_link;
   fo.chmod = fskit_fuse_chmod;
   fo.chown = fskit_fuse_chown;
   fo.truncate = fskit_fuse_truncate;
   fo.utime = fskit_fuse_utime;
   fo.open = fskit_fuse_open;
   fo.read = fskit_fuse_read;
   fo.write = fskit_fuse_write;
   fo.statfs = fskit_fuse_statfs;
   fo.flush = fskit_fuse_flush;
   fo.release = fskit_fuse_release;
   fo.fsync = fskit_fuse_fsync;
   fo.setxattr = fskit_fuse_setxattr;
   fo.getxattr = fskit_fuse_getxattr;
   fo.listxattr = fskit_fuse_listxattr;
   fo.removexattr = fskit_fuse_removexattr;
   fo.opendir = fskit_fuse_opendir;
   fo.readdir = fskit_fuse_readdir;
   fo.releasedir = fskit_fuse_releasedir;
   fo.fsyncdir = fskit_fuse_fsyncdir;
   fo.init = fskit_fuse_fuse_init;
   fo.access = fskit_fuse_access;
   fo.create = fskit_fuse_create;
   fo.ftruncate = fskit_fuse_ftruncate;
   fo.fgetattr = fskit_fuse_fgetattr;

   return fo;
}


// set up fskit for FUSE
int fskit_fuse_init( struct fskit_fuse_state* state, void* core_state ) {

   int rc = 0;

   // set up library
   rc = fskit_library_init();
   if( rc != 0 ) {
      fskit_error( "fskit_library_init rc = %d\n", rc );
      return rc;
   }

   // set up fskit
   struct fskit_core* core = (struct fskit_core*)calloc( sizeof(struct fskit_core), 1 );

   rc = fskit_core_init( core, core_state );
   if( rc != 0 ) {
      fskit_error( "fskit_core_init rc = %d\n", rc );
      return rc;
   }

   memset( state, 0, sizeof(struct fskit_fuse_state) );

   state->core = core;

   return 0;
}

// run fskit with fuse
int fskit_fuse_main( struct fskit_fuse_state* state, int argc, char** argv ) {

   int rc = 0;

   // set up FUSE
   struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
   struct fuse_operations fskit_fuse = fskit_fuse_get_opers();
   struct fuse_chan* ch = NULL;
   struct fuse* fs = NULL;
   int multithreaded = 1;
   int foreground = 0;
   char* mountpoint = NULL;

   // parse command-line...
   rc = fuse_parse_cmdline( &args, &mountpoint, &multithreaded, &foreground );
   if( rc < 0 ) {

      fskit_error("fuse_parse_cmdline rc = %d\n", rc );
      fuse_opt_free_args(&args);

      return rc;
   }

   state->mountpoint = strdup( mountpoint );

   // mount
   ch = fuse_mount( mountpoint, &args );
   if( ch == NULL ) {

      rc = -errno;
      fskit_error("fuse_mount failed, errno = %d\n", rc );

      fuse_opt_free_args(&args);

      if( rc == 0 ) {
          rc = -EPERM;
      }

      return rc;
   }

   // create the filesystem
   fs = fuse_new( ch, &args, &fskit_fuse, sizeof(fskit_fuse), state );
   fuse_opt_free_args(&args);

   if( fs == NULL ) {

      // failed
      rc = -errno;
      fskit_error("fuse_new failed, errno = %d\n", rc );

      fuse_unmount( mountpoint, ch );

      if( rc == 0 ) {
          rc = -EPERM;
      }

      return rc;
   }

   // daemonize if running in the background
   rc = fuse_daemonize( foreground );
   if( rc != 0 ) {

      // failed
      fskit_error("fuse_daemonize(%d) rc = %d\n", foreground, rc );

      fuse_unmount( mountpoint, ch );
      fuse_destroy( fs );

      return rc;
   }

   // set up FUSE signal handlers
   rc = fuse_set_signal_handlers( fuse_get_session(fs) );
   if( rc < 0 ) {

      // failed
      fskit_error("fuse_set_signal_handlers rc = %d\n", rc );

      fuse_unmount( mountpoint, ch );
      fuse_destroy( fs );
      return rc;
   }

   // if we have a post-mount callback, call it now, since FUSE is ready to receive requests
   if( state->postmount != NULL ) {

      rc = (*state->postmount)( state, state->postmount_cls );
      if( rc != 0 ) {

         fskit_error("fskit postmount callback rc = %d\n", rc );

         fuse_unmount( mountpoint, ch );
         fuse_destroy( fs );

         return rc;
      }
   }

   // run the filesystem--start processing requests
   if( multithreaded ) {
      rc = fuse_loop_mt( fs );
   }
   else {
      rc = fuse_loop( fs );
   }

   fuse_teardown( fs, mountpoint );

   return rc;
}

// shut down fskit fuse
int fskit_fuse_shutdown( struct fskit_fuse_state* state, void** core_state ) {

   struct fskit_core* core = state->core;
   int rc = 0;

   // clean up
   // blow away all inodes
   rc = fskit_detach_all( core, "/", core->root.children );
   if( rc != 0 ) {
      fskit_error( "fskit_detach_all(\"/\") rc = %d\n", rc );
   }

   // destroy the core
   rc = fskit_core_destroy( core, core_state );
   if( rc != 0 ) {
      fskit_error( "fskit_core_destroy rc = %d\n", rc );
   }

   // shut down the library
   rc = fskit_library_shutdown();
   if( rc != 0 ) {
      fskit_error( "fskit_library_shutdown rc = %d\n", rc );
   }

   free( core );

   // free mountpoint
   if( state->mountpoint != NULL ) {
      free( state->mountpoint );
      state->mountpoint = NULL;
   }

   return rc;
}

// get the fskit core from the state
struct fskit_core* fskit_fuse_get_core( struct fskit_fuse_state* state ) {
   return state->core;
}
