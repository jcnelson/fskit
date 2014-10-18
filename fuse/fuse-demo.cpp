/*
   fskit_fuse: a FUSE filesystem demo of fskit
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

#include "fuse-demo.h"

// get running state
struct fskit_fuse_state* fskit_fuse_get_state() {
   return (struct fskit_fuse_state*)fuse_get_context()->private_data;
}

// get caller UID 
uint64_t fskit_fuse_get_uid() {
   return fuse_get_context()->uid;
}

// get caller GID 
uint64_t fskit_fuse_get_gid() {
   return fuse_get_context()->gid;
}

// get caller PID 
pid_t fskit_fuse_get_pid() {
   return fuse_get_context()->pid;
}

// get caller umask 
mode_t fskit_fuse_get_umask() {
   return fuse_get_context()->umask;
}

// make a FUSE file info for a file handle
struct fskit_fuse_file_info* fskit_fuse_make_file_handle( struct fskit_file_handle* fh ) {
   
   struct fskit_fuse_file_info* ffi = CALLOC_LIST( struct fskit_fuse_file_info, 1 );
   if( ffi == NULL ) {
      return NULL;
   }
   
   ffi->type = FSKIT_ENTRY_TYPE_FILE;
   ffi->handle.fh = fh;
   
   return ffi;
}

// make a FUSE file info for a dir handle
struct fskit_fuse_file_info* fskit_fuse_make_dir_handle( struct fskit_dir_handle* dh ) {
   
   struct fskit_fuse_file_info* ffi = CALLOC_LIST( struct fskit_fuse_file_info, 1 );
   if( ffi == NULL ) {
      return NULL;
   }
   
   ffi->type = FSKIT_ENTRY_TYPE_DIR;
   ffi->handle.dh = dh;
   
   return ffi;
}

int fskit_fuse_getattr(const char *path, struct stat *statbuf) {
   
   dbprintf("getattr(%s, %p)\n", path, statbuf );
   
   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uint64_t uid = fskit_fuse_get_uid();
   uint64_t gid = fskit_fuse_get_gid();
   
   int rc = fskit_stat( state->core, path, uid, gid, statbuf );
   
   dbprintf("getattr(%s, %p) rc = %d\n", path, statbuf, rc );
   return rc;
}


int fskit_fuse_readlink(const char *path, char *link, size_t size) {
   
   dbprintf("readlink(%s, %s, %zu)\n", path, link, size );
   
   // not supported by fskit
   
   dbprintf("readlink(%s, %s, %zu) rc = %d\n", path, link, size, -ENOSYS );
   return -ENOSYS;
}


int fskit_fuse_mknod(const char *path, mode_t mode, dev_t dev) {
   
   dbprintf("mknod(%s, %o, %d, %d)\n", path, mode, major(dev), minor(dev) );
   
   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uint64_t uid = fskit_fuse_get_uid();
   uint64_t gid = fskit_fuse_get_gid();
   
   int rc = fskit_mknod( state->core, path, mode, dev, uid, gid );
   
   dbprintf("mknod(%s, %o, %d, %d) rc = %d\n", path, mode, major(dev), minor(dev), rc );
   
   return rc;
}

int fskit_fuse_mkdir(const char *path, mode_t mode) {
   
   dbprintf("mkdir(%s, %o)\n", path, mode );
   
   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uint64_t uid = fskit_fuse_get_uid();
   uint64_t gid = fskit_fuse_get_gid();
   
   int rc = fskit_mkdir( state->core, path, mode, uid, gid );
   
   dbprintf("mkdir(%s, %o) rc = %d\n", path, mode, rc );
   
   return rc;
}

int fskit_fuse_unlink(const char *path) {
   
   dbprintf("unlink(%s)\n", path );
   
   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uint64_t uid = fskit_fuse_get_uid();
   uint64_t gid = fskit_fuse_get_gid();
   
   int rc = fskit_unlink( state->core, path, uid, gid );
   
   dbprintf("unlink(%s) rc = %d\n", path, rc );
   
   return rc;
}

int fskit_fuse_rmdir(const char *path) {
   
   dbprintf("rmdir(%s)\n", path );
   
   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uint64_t uid = fskit_fuse_get_uid();
   uint64_t gid = fskit_fuse_get_gid();
   
   int rc = fskit_rmdir( state->core, path, uid, gid );
   
   dbprintf("rmdir(%s) rc = %d\n", path, rc );
   
   return rc;
}

int fskit_fuse_symlink(const char *path, const char *link) {
   
   dbprintf("symlink(%s, %s)\n", path, link );
   
   // not supported by fskit
   dbprintf("symlink(%s, %s) rc = %d\n", path, link, -ENOSYS );
   return -ENOSYS;
}

int fskit_fuse_rename(const char *path, const char *newpath) {
   
   dbprintf("rename(%s, %s)\n", path, newpath );
   
   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uint64_t uid = fskit_fuse_get_uid();
   uint64_t gid = fskit_fuse_get_gid();
   
   int rc = fskit_rename( state->core, path, newpath, uid, gid );
   
   dbprintf("rename(%s, %s) rc = %d\n", path, newpath, rc );
   
   return rc;
}

int fskit_fuse_link(const char *path, const char *newpath) {
   
   dbprintf("link(%s, %s)\n", path, newpath );
   // not supported by fskit 
   dbprintf("link(%s, %s) rc = %d\n", path, newpath, -ENOSYS );
   
   return -ENOSYS;
}

int fskit_fuse_chmod(const char *path, mode_t mode) {
   
   dbprintf("chmod(%s, %o)\n", path, mode );
   
   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uint64_t uid = fskit_fuse_get_uid();
   uint64_t gid = fskit_fuse_get_gid();
   
   int rc = fskit_chmod( state->core, path, uid, gid, mode );
   
   dbprintf("chmod(%s, %o) rc = %d\n", path, mode, rc );
   
   return rc;
}

int fskit_fuse_chown(const char *path, uid_t new_uid, gid_t new_gid) {
   
   dbprintf("chown(%s, %d, %d)\n", path, new_uid, new_gid );
   
   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uint64_t uid = fskit_fuse_get_uid();
   uint64_t gid = fskit_fuse_get_gid();
   
   int rc = fskit_chown( state->core, path, uid, gid, new_uid, new_gid );
   
   dbprintf("chown(%s, %d, %d) rc = %d\n", path, new_uid, new_gid, rc );
   
   return rc;
}

int fskit_fuse_truncate(const char *path, off_t newsize) {
   
   dbprintf("truncate(%s, %jd)\n", path, newsize );
   
   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uint64_t uid = fskit_fuse_get_uid();
   uint64_t gid = fskit_fuse_get_gid();
   
   int rc = fskit_trunc( state->core, path, uid, gid, newsize );
   
   dbprintf("truncate(%s, %jd) rc = %d\n", path, newsize, rc );
   
   return rc;
}

int fskit_fuse_utime(const char *path, struct utimbuf *ubuf) {
   
   dbprintf("utime(%s, %ld.%ld)\n", path, ubuf->actime, ubuf->modtime );
   
   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uint64_t uid = fskit_fuse_get_uid();
   uint64_t gid = fskit_fuse_get_gid();
   
   int rc = fskit_utime( state->core, path, uid, gid, ubuf );
   
   dbprintf("utime(%s, %ld.%ld) rc = %d\n", path, ubuf->actime, ubuf->modtime, rc );
   
   return rc;
}

int fskit_fuse_open(const char *path, struct fuse_file_info *fi) {
   
   dbprintf("open(%s, %p)\n", path, fi);
   
   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uint64_t uid = fskit_fuse_get_uid();
   uint64_t gid = fskit_fuse_get_gid();
   uint64_t umask = fskit_fuse_get_umask();
   struct fskit_fuse_file_info* ffi = NULL;
   int rc = 0;
   
   struct fskit_file_handle* fh = fskit_open( state->core, path, uid, gid, fi->flags, ~umask, &rc );
   
   if( rc != 0 ) {
      
      dbprintf("open(%s, %p) rc = %d\n", path, fi, rc);
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
   
   dbprintf("open(%s, %p) rc = %d\n", path, fi, rc);
   
   return rc;
}

int fskit_fuse_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
   
   dbprintf("read(%s, %p, %zu, %jd, %p)\n", path, buf, size, offset, fi);
   
   struct fskit_fuse_state* state = fskit_fuse_get_state();
   
   struct fskit_file_handle* fh = (struct fskit_file_handle*)fi->fh;
   ssize_t num_read = 0;
   
   num_read = fskit_read( state->core, fh, buf, size, offset );

   dbprintf("read(%s, %p, %zu, %jd, %p) rc = %d\n", path, buf, size, offset, fi, num_read);
      
   return (int)num_read;
}

int fskit_fuse_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
   
   dbprintf("write(%s, %p, %zu, %jd, %p)\n", path, buf, size, offset, fi);
   
   struct fskit_fuse_state* state = fskit_fuse_get_state();
   
   struct fskit_file_handle* fh = (struct fskit_file_handle*)fi->fh;
   ssize_t num_written = 0;
   
   num_written = fskit_write( state->core, fh, buf, size, offset );
   
   dbprintf("write(%s, %p, %zu, %jd, %p) rc = %d\n", path, buf, size, offset, fi, num_written);
   
   return (int)num_written;
}

int fskit_fuse_statfs(const char *path, struct statvfs *statv) {
   
   dbprintf("statfs(%s, %p)\n", path, statv );
   
   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uint64_t uid = fskit_fuse_get_uid();
   uint64_t gid = fskit_fuse_get_gid();
   
   int rc = fskit_statvfs( state->core, path, uid, gid, statv );
   
   dbprintf("statfs(%s, %p) rc = %d\n", path, statv, rc );
   
   return rc;
}

int fskit_fuse_flush(const char *path, struct fuse_file_info *fi) {
   
   dbprintf("flush(%s, %p)\n", path, fi);
   
   // not addressed by fskit
   
   dbprintf("flush(%s, %p) rc = %d\n", path, fi, 0);
   return 0;
}

int fskit_fuse_release(const char *path, struct fuse_file_info *fi) {
   
   dbprintf("release(%s, %p)\n", path, fi);
   
   struct fskit_fuse_state* state = fskit_fuse_get_state();
   
   struct fskit_fuse_file_info* ffi = (struct fskit_fuse_file_info*)fi->fh;
   
   int rc = fskit_close( state->core, ffi->handle.fh );
   
   if( rc == 0 ) {
      safe_free( ffi );
   }
   
   dbprintf("release(%s, %p) rc = %d\n", path, fi, rc);
   return rc;
}

int fskit_fuse_fsync(const char *path, int datasync, struct fuse_file_info *fi) {
   
   dbprintf("fsync(%s, %d, %p)\n", path, datasync, fi );
   
   // not addressed by fskit
   
   dbprintf("fsync(%s, %d, %p) rc = %d\n", path, datasync, fi, 0 );
   return 0;
}

int fskit_fuse_setxattr(const char *path, const char *name, const char *value, size_t size, int flags) {
   
   dbprintf("setxattr(%s, %s, %p, %zu, %X)\n", path, name, value, size, flags );
   
   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uint64_t uid = fskit_fuse_get_uid();
   uint64_t gid = fskit_fuse_get_gid();
   
   int rc = fskit_setxattr( state->core, path, uid, gid, name, value, size, flags );
   
   dbprintf("setxattr(%s, %s, %p, %zu, %X) rc = %d\n", path, name, value, size, flags, rc );
   return rc;
}

int fskit_fuse_getxattr(const char *path, const char *name, char *value, size_t size) {
   
   dbprintf("getxattr(%s, %s, %p, %zu)\n", path, name, value, size );
   
   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uint64_t uid = fskit_fuse_get_uid();
   uint64_t gid = fskit_fuse_get_gid();
   
   int rc = fskit_getxattr( state->core, path, uid, gid, name, value, size );
   
   dbprintf("getxattr(%s, %s, %p, %zu) rc = %d\n", path, name, value, size, rc );
   return rc;
}

int fskit_fuse_listxattr(const char *path, char *list, size_t size) {
   
   dbprintf("listxattr(%s, %p, %zu)\n", path, list, size );
   
   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uint64_t uid = fskit_fuse_get_uid();
   uint64_t gid = fskit_fuse_get_gid();
   
   int rc = fskit_listxattr( state->core, path, uid, gid, list, size );
   
   dbprintf("listxattr(%s, %p, %zu) rc = %d\n", path, list, size, rc );
   return rc;
}

int fskit_fuse_removexattr(const char *path, const char *name) {
   
   dbprintf("removexattr(%s, %s)\n", path, name );
   
   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uint64_t uid = fskit_fuse_get_uid();
   uint64_t gid = fskit_fuse_get_gid();
   
   int rc = fskit_removexattr( state->core, path, uid, gid, name );
   
   dbprintf("removexattr(%s, %s) rc = %d\n", path, name, rc );
   return rc;
}

int fskit_fuse_opendir(const char *path, struct fuse_file_info *fi) {
   
   dbprintf("opendir(%s, %p)\n", path, fi );
   
   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uint64_t uid = fskit_fuse_get_uid();
   uint64_t gid = fskit_fuse_get_gid();
   struct fskit_fuse_file_info* ffi = NULL;
   int rc = 0;
   
   struct fskit_dir_handle* dh = fskit_opendir( state->core, path, uid, gid, &rc );
   
   if( rc != 0 ) {
      
      dbprintf("opendir(%s, %p) rc = %d\n", path, fi, rc );
      return rc;
   }
   
   ffi = fskit_fuse_make_dir_handle( dh );
   
   if( ffi == NULL ) {
      fskit_closedir( state->core, dh );
      
      dbprintf("opendir(%s, %p) rc = %d\n", path, fi, -ENOMEM );
      return -ENOMEM;
   }
   
   fi->fh = (uint64_t)ffi; 
   
   dbprintf("opendir(%s, %p) rc = %d\n", path, fi, 0 );
   return 0;
}

int fskit_fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
   
   dbprintf("readdir(%s, %jd, %p, %p)\n", path, offset, buf, fi );
   
   struct fskit_fuse_state* state = fskit_fuse_get_state();
   struct fskit_dir_handle* fdh = NULL;
   struct fskit_fuse_file_info* ffi = NULL;
   int rc = 0;
   uint64_t num_read = 0;
   
   ffi = (struct fskit_fuse_file_info*)fi->fh;
   fdh = ffi->handle.dh;
   
   struct fskit_dir_entry** dirents = fskit_listdir( state->core, fdh, &num_read, &rc );
   
   if( dirents == NULL || rc != 0 ) {
      dbprintf("readdir(%s, %jd, %p, %p) rc = %d\n", path, offset, buf, fi, rc );
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
   
   dbprintf("readdir(%s, %jd, %p, %p) rc = %d\n", path, offset, buf, fi, rc );
   return rc;
}

int fskit_fuse_releasedir(const char *path, struct fuse_file_info *fi) {
   
   dbprintf("releasedir(%s, %p)\n", path, fi );
   
   struct fskit_fuse_state* state = fskit_fuse_get_state();
   struct fskit_fuse_file_info* ffi = NULL;
   int rc = 0;
   
   ffi = (struct fskit_fuse_file_info*)fi->fh;
   
   rc = fskit_closedir( state->core, ffi->handle.dh );
   
   if( rc == 0 ) {
      safe_free( ffi );
   }
   
   dbprintf("releasedir(%s, %p) rc = %d\n", path, fi, rc );
   
   return rc;
}

int fskit_fuse_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi) {
   
   dbprintf("fsyncdir(%s, %d, %p)\n", path, datasync, fi);
   
   // not addressed by fskit
   
   dbprintf("fsyncdir(%s, %d, %p) rc = %d\n", path, datasync, fi, 0);
   return 0;
}

int fskit_fuse_access(const char *path, int mask) {
   
   dbprintf("access(%s, %X)\n", path, mask );
   
   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uint64_t uid = fskit_fuse_get_uid();
   uint64_t gid = fskit_fuse_get_gid();
   
   int rc = fskit_access( state->core, path, uid, gid, mask );
   
   dbprintf("access(%s, %X) rc = %d\n", path, mask, rc );
   
   return rc;
}

int fskit_fuse_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
   
   dbprintf("create(%s, %o, %p)\n", path, mode, fi );
   
   struct fskit_fuse_state* state = fskit_fuse_get_state();
   uint64_t uid = fskit_fuse_get_uid();
   uint64_t gid = fskit_fuse_get_gid();
   struct fskit_fuse_file_info* ffi = NULL;
   int rc = 0;
   
   struct fskit_file_handle* fh = fskit_create( state->core, path, uid, gid, mode, &rc );
   
   if( rc != 0 ) {
      
      dbprintf("create(%s, %o, %p) rc = %d\n", path, mode, fi, rc );
   
      return rc;
   }
   
   ffi = fskit_fuse_make_file_handle( fh );
   if( ffi == NULL ) {
      
      fskit_close( state->core, fh );
      
      dbprintf("create(%s, %o, %p) rc = %d\n", path, mode, fi, -ENOMEM );
   
      return -ENOMEM;
   }
   
   fi->fh = (uint64_t)ffi;
   
   // NOTE: fskit_read() and fskit_write() return a negative error code on error,
   // so set direct_io to allow this error code to be propagated.
   fi->direct_io = 1;
   
   dbprintf("create(%s, %o, %p) rc = %d\n", path, mode, fi, rc );
   
   return rc;
}

int fskit_fuse_ftruncate(const char *path, off_t new_size, struct fuse_file_info *fi) {
   
   dbprintf("ftruncate(%s, %zu, %p)\n", path, new_size, fi );
   
   struct fskit_fuse_state* state = fskit_fuse_get_state();
   struct fskit_fuse_file_info* ffi = NULL;
   
   ffi = (struct fskit_fuse_file_info*)fi->fh;
   
   int rc = fskit_ftrunc( state->core, ffi->handle.fh, new_size );
   
   dbprintf("ftruncate(%s, %zu, %p) rc = %d\n", path, new_size, fi, rc );
   
   return rc;
}

int fskit_fuse_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi) {
   
   dbprintf("fgetattr(%s, %p, %p)\n", path, statbuf, fi );
   
   struct fskit_fuse_state* state = fskit_fuse_get_state();
   struct fskit_fuse_file_info* ffi = NULL;
   
   ffi = (struct fskit_fuse_file_info*)fi->fh;
   
   int rc = 0;
   
   if( ffi->type == FSKIT_ENTRY_TYPE_FILE ) {
      rc = fskit_fstat( state->core, ffi->handle.fh->fent, statbuf );
   }
   else {
      rc = fskit_fstat( state->core, ffi->handle.dh->dent, statbuf );
   }
   
   dbprintf("fgetattr(%s, %p, %p) rc = %d\n", path, statbuf, fi, rc );
   
   return rc;
}

void *fskit_fuse_fuse_init(struct fuse_conn_info *conn) {
   return fskit_fuse_get_state();
}

void fskit_fuse_destroy(void *userdata) {
   return;
}

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


int main( int argc, char** argv ) {
   
   struct fskit_fuse_state state;
   int rc = 0;
   
   // set up library
   rc = fskit_library_init();
   if( rc != 0 ) {
      fprintf( stderr, "fskit_library_init rc = %d\n", rc );
      exit(1);
   }
   
   // set up fskit 
   struct fskit_core* core = CALLOC_LIST( struct fskit_core, 1 );
   
   rc = fskit_core_init( core, NULL );
   if( rc != 0 ) {
      fprintf( stderr, "fskit_core_init rc = %d\n", rc );
      exit(1);
   }
   
   // set up FUSE
   struct fuse_operations fskit_fuse = fskit_fuse_get_opers();
   
   memset( &state, 0, sizeof(struct fskit_fuse_state) );
   
   state.core = core;
   
   // run!
   rc = fuse_main(argc, argv, &fskit_fuse, &state );

   // clean up
   // blow away all inodes
   rc = fskit_detach_all( core, "/", core->root.children );
   if( rc != 0 ) {
      fprintf( stderr, "fskit_detach_all(\"/\") rc = %d\n", rc );
   }
   
   // destroy the core
   rc = fskit_core_destroy( core, NULL );
   if( rc != 0 ) {
      fprintf( stderr, "fskit_core_destroy rc = %d\n", rc );
   }
   
   // shut down the library
   rc = fskit_library_shutdown();
   if( rc != 0 ) {
      fprintf( stderr, "fskit_library_shutdown rc = %d\n", rc );
   }
   
   free( core );
   
   return rc;
}
