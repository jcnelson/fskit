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

#include "test-route.h"

int create_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, mode_t mode, void** inode_data, void** handle_data ) {
   fskit_debug("Create %" PRIX64 " (%s) mode=%o\n", fent->file_id, route_metadata->path, mode );
   return 0;
}

int mknod_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, mode_t mode, dev_t dev, void** inode_data ) {
   fskit_debug("Mknod %" PRIX64 " (%s) mode=%o dev=%lX \n", fent->file_id, route_metadata->path, mode, dev );
   return 0;
}

int mkdir_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* dent, mode_t mode, void** inode_data ) {
   fskit_debug("Mkdir %" PRIX64 " (%s) mode=%o\n", dent->file_id, route_metadata->path, mode );
   return 0;
}

int open_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, int flags, void** handle_data ) {
   fskit_debug("Open %" PRIX64 " (%s) flags=%X\n", fent->file_id, route_metadata->path, flags );
   return 0;
}

int close_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, void* handle_data ) {
   fskit_debug("Close %" PRIX64 " (%s)\n", fent->file_id, route_metadata->path );
   return 0;
}

int read_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, char* buf, size_t buflen, off_t offset, void* handle_data ) {
   fskit_debug("Read %" PRIX64 " (%s) buf=%p buflen=%zu offset=%jd handle_data=%p\n", fent->file_id, route_metadata->path, buf, buflen, offset, handle_data );
   return buflen;
}

int write_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, char* buf, size_t buflen, off_t offset, void* handle_data ) {
   fskit_debug("Write %" PRIX64 " (%s) buf=%p buflen=%zu offset=%jd handle_data=%p\n", fent->file_id, route_metadata->path, buf, buflen, offset, handle_data );
   return buflen;
}

int trunc_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, off_t new_size, void* handle_data ) {
   fskit_debug("Trunc %" PRIX64 " (%s) new_size=%jd handle_data=%p\n", fent->file_id, route_metadata->path, new_size, handle_data );
   return 0;
}

int readdir_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* dir, struct fskit_dir_entry** dents, size_t num_dents ) {
   fskit_debug("Readdir %" PRIX64 " (%s) dents[0]=(%" PRIX64 " %s), num_dents=%" PRIu64 "\n", dents[0]->file_id, route_metadata->path, dents[0]->file_id, dents[0]->name, num_dents );
   return 0;
}

int detach_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, void* inode_data ) {
   fskit_debug("Detach %" PRIX64 " (%s)\n", fent->file_id, route_metadata->path );
   return 0;
}

int stat_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, struct stat* sb ) {
   fskit_debug("Stat %" PRIX64 " (%s)\n", fent->file_id, route_metadata->path );
   return 0;
}

int sync_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent ) {
   fskit_debug("Sync %" PRIX64 " (%s)\n", fent->file_id, route_metadata->path );
   return 0;
}

int rename_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, char const* new_path, struct fskit_entry* new_fent ) {
   fskit_debug("Rename %" PRIX64 " from %s to %s (overwriting %s)\n", fent->file_id, route_metadata->path, new_path, (new_fent != NULL ? new_fent->name : "(null)"));
   return 0;
}

int main( int argc, char** argv ) {
   struct fskit_core core;
   int rc;
   void* output;
   
   int create_rh, mknod_rh, mkdir_rh, opendir_rh, open_rh, close_rh, closedir_rh, readdir_rh, read_rh, write_rh, trunc_rh, unlink_rh, rmdir_rh, stat_rh, sync_rh, rename_rh;

   rc = fskit_test_begin( &core, NULL );
   if( rc != 0 ) {
      exit(1);
   }

   // install routes
   create_rh = fskit_route_create( &core, "/test-file", create_cb, FSKIT_SEQUENTIAL );
   if( create_rh < 0 ) {
      fskit_error("fskit_route_create rc = %d\n", create_rh );
      exit(1);
   }

   mknod_rh = fskit_route_mknod( &core, "/test-node", mknod_cb, FSKIT_SEQUENTIAL );
   if( mknod_rh < 0 ) {
      fskit_error("fskit_route_mknod rc = %d\n", mknod_rh );
      exit(1);

   }

   mkdir_rh = fskit_route_mkdir( &core, "/test-dir", mkdir_cb, FSKIT_SEQUENTIAL );
   if( mkdir_rh < 0 ) {
      fskit_error("fskit_route_mkdir rc = %d\n", mkdir_rh );
      exit(1);
   }

   opendir_rh = fskit_route_open( &core, "/test-dir", open_cb, FSKIT_SEQUENTIAL );
   if( opendir_rh < 0 ) {
      fskit_error("fskit_route_open rc = %d\n", opendir_rh );
      exit(1);
   }

   open_rh = fskit_route_open( &core, "/test-file", open_cb, FSKIT_SEQUENTIAL );
   if( open_rh < 0 ) {
      fskit_error("fskit_route_open rc = %d\n", open_rh );
      exit(1);
   }

   close_rh = fskit_route_close( &core, "/test-file", close_cb, FSKIT_SEQUENTIAL );
   if( close_rh < 0 ) {
      fskit_error("fskit_route_close rc = %d\n", close_rh );
      exit(1);
   }

   closedir_rh = fskit_route_close( &core, "/test-dir", close_cb, FSKIT_SEQUENTIAL );
   if( closedir_rh < 0 ) {
      fskit_error("fskit_route_close rc = %d\n", closedir_rh );
      exit(1);
   }

   readdir_rh = fskit_route_readdir( &core, "/test-dir", readdir_cb, FSKIT_SEQUENTIAL );
   if( readdir_rh < 0 ) {
      fskit_error("fskit_route_readdir rc = %d\n", readdir_rh );
      exit(1);
   }

   read_rh = fskit_route_read( &core, "/test-file", read_cb, FSKIT_SEQUENTIAL );
   if( read_rh < 0 ) {
      fskit_error("fskit_route_read rc = %d\n", read_rh );
      exit(1);
   }

   write_rh = fskit_route_write( &core, "/test-file", write_cb, FSKIT_SEQUENTIAL );
   if( write_rh < 0 ) {
      fskit_error("fskit_route_write rc = %d\n", write_rh );
      exit(1);
   }

   trunc_rh = fskit_route_trunc( &core, "/test-file", trunc_cb, FSKIT_SEQUENTIAL );
   if( trunc_rh < 0 ) {
      fskit_error("fskit_route_trunc rc = %d\n", trunc_rh );
      exit(1);
   }

   unlink_rh = fskit_route_detach( &core, "/test-file", detach_cb, FSKIT_SEQUENTIAL );
   if( unlink_rh < 0 ) {
      fskit_error("fskit_route_detach rc = %d\n", unlink_rh );
      exit(1);
   }

   rmdir_rh = fskit_route_detach( &core, "/test-dir", detach_cb, FSKIT_SEQUENTIAL );
   if( rmdir_rh < 0 ) {
      fskit_error("fskit_route_detach rc = %d\n", rmdir_rh );
      exit(1);
   }

   stat_rh = fskit_route_stat( &core, "/test-file", stat_cb, FSKIT_SEQUENTIAL );
   if( stat_rh < 0 ) {
      fskit_error("fskit_route_stat rc = %d\n", stat_rh );
      exit(1);
   }

   sync_rh = fskit_route_sync( &core, "/test-file", sync_cb, FSKIT_SEQUENTIAL );
   if( sync_rh < 0 ) {
      fskit_error("fskit_route_sync rc = %d\n", sync_rh );
      exit(1);
   }
   
   rename_rh = fskit_route_rename( &core, "/test-file", rename_cb, FSKIT_SEQUENTIAL );
   if( rename_rh < 0 ) {
      fskit_error("fskit_route_rename rc = %d\n", rename_rh );
      exit(1);
   }

   // invoke routes
   struct fskit_file_handle* fh = NULL;
   struct fskit_file_handle* fh2 = NULL;
   struct fskit_dir_handle* dh = NULL;
   struct fskit_dir_entry** dents = NULL;
   uint64_t num_dents = 0;
   char const* write_buf = "foo";
   char read_buf[10];
   size_t read_len = 10;
   size_t write_len = strlen(write_buf) + 1;
   struct stat sb;

   // mkdir route
   rc = fskit_mkdir( &core, "/test-dir", 0755, 0, 0 );
   if( rc != 0 ) {
      fskit_error("fskit_mkdir rc = %d\n", rc );
      exit(1);
   }

   // opendir route
   dh = fskit_opendir( &core, "/test-dir", 0, 0, &rc );
   if( rc != 0 ) {
      fskit_error("fskit_opendir rc = %d\n", rc );
      exit(1);

   }

   // readdir route
   dents = fskit_listdir( &core, dh, &num_dents, &rc );
   if( rc != 0 ) {
      fskit_error("fskit_listdir rc = %d\n", rc );
      exit(1);
   }

   fskit_dir_entry_free_list( dents );

   // closedir (close) route
   rc = fskit_closedir( &core, dh );
   if( rc != 0 ) {
      fskit_error("fskit_closedir rc = %d\n", rc );
      exit(1);
   }

   // create AND truncate route
   fh = fskit_create( &core, "/test-file", 0, 0, 0644, &rc );
   if( rc != 0 ) {
      fskit_error("fskit_create rc = %d\n", rc );
      exit(1);
   }

   // open route
   fh2 = fskit_open( &core, "/test-file", 0, 0, O_RDONLY, 0, &rc );
   if( rc != 0 ) {
      fskit_error("fskit_open rc = %d\n", rc );
      exit(1);
   }

   // write route
   rc = fskit_write( &core, fh, (char*)write_buf, write_len, 0 );
   if( rc != (signed)write_len ) {
      fskit_error("fskit_write rc = %d\n", rc );
      exit(1);
   }

   // read route
   rc = fskit_read( &core, fh2, read_buf, 10, 0 );
   if( rc != (signed)read_len ) {
      fskit_error("fskit_read rc = %d\n", rc );
      exit(1);
   }

   // sync route
   rc = fskit_fsync( &core, fh );
   if( rc != 0 ) {
      fskit_error("fskit_sync rc = %d\n", rc );
      exit(1);
   }

   // close route
   rc = fskit_close( &core, fh );
   if( rc != 0 ) {
      fskit_error("fskit_close rc = %d\n", rc );
      exit(1);
   }

   // close route
   rc = fskit_close( &core, fh2 );
   if( rc != 0 ) {
      fskit_error("fskit_close rc = %d\n", rc );
      exit(1);
   }

   // mkod route
   rc = fskit_mknod( &core, "/test-node", S_IFBLK | 0644, makedev( 1, 9 ), 0, 0 );
   if( rc != 0 ) {
      fskit_error("fskit_mknod rc = %d\n", rc );
      exit(1);
   }

   // stat route
   rc = fskit_stat( &core, "/test-file", 0, 0, &sb );
   if( rc != 0 ) {
      fskit_error("fskit_stat rc = %d\n", rc );
      exit(1);
   }
   
   // rename 
   rc = fskit_rename( &core, "/test-file", "/test-file-renamed", 0, 0 );
   if( rc != 0 ) {
      fskit_error("fskit_rename rc = %d\n", rc );
      exit(1);
   }

   // undefine routes
   rc = fskit_unroute_create( &core, create_rh );
   if( rc != 0 ) {
      fskit_error("fskit_unroute_create rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_unroute_mknod( &core, mknod_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_mknod rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_unroute_mkdir( &core, mkdir_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_mkdir rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_unroute_open( &core, open_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_open rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_unroute_open( &core, opendir_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_open rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_unroute_close( &core, close_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_close rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_unroute_close( &core, closedir_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_close rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_unroute_readdir( &core, readdir_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_readdir rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_unroute_read( &core, read_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_read rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_unroute_write( &core, write_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_write rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_unroute_trunc( &core, trunc_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_trunc rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_unroute_detach( &core, unlink_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_detach rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_unroute_detach( &core, rmdir_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_detach rc = %d\n", rc );
      exit(1);
   }
   
   rc = fskit_unroute_rename( &core, rename_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_rename rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_test_end( &core, &output );
   if( rc != 0 ) {
      fskit_error("fskit_test_end rc = %d\n", rc );
      exit(1);
   }
   return 0;
}
