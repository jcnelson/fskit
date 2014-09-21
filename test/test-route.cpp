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

#include "test-route.h"

int create_cb( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, int flags, mode_t mode, void** inode_data, void** handle_data ) {
   dbprintf("Create %" PRIX64 " (%s) mode=%o flags=%X\n", fent->file_id, grp->path, mode, flags );
   return 0;
}

int mknod_cb( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, mode_t mode, dev_t dev, void** inode_data ) {
   dbprintf("Mknod %" PRIX64 " (%s) mode=%o dev=%lX \n", fent->file_id, grp->path, mode, dev );
   return 0;
}

int mkdir_cb( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* dent, mode_t mode, void** inode_data ) {
   dbprintf("Mkdir %" PRIX64 " (%s) mode=%o\n", dent->file_id, grp->path, mode );
   return 0;
}

int open_cb( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, int flags, void** handle_data ) {
   dbprintf("Open %" PRIX64 " (%s) flags=%X\n", fent->file_id, grp->path, flags );
   return 0;
}

int close_cb( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, void* handle_data ) {
   dbprintf("Close %" PRIX64 " (%s)\n", fent->file_id, grp->path );
   return 0;
}

int read_cb( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, char* buf, size_t buflen, off_t offset ) {
   dbprintf("Read %" PRIX64 " (%s) buf=%p buflen=%zu offset=%jd\n", fent->file_id, grp->path, buf, buflen, offset );
   return buflen;
}

int write_cb( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, char* buf, size_t buflen, off_t offset ) {
   dbprintf("Write %" PRIX64 " (%s) buf=%p buflen=%zu offset=%jd\n", fent->file_id, grp->path, buf, buflen, offset );
   return buflen;
}

int trunc_cb( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, off_t new_size ) {
   dbprintf("Trunc %" PRIX64 " (%s) new_size=%jd\n", fent->file_id, grp->path, new_size );
   return 0;
}

int readdir_cb( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* dir, struct fskit_dir_entry* dent ) {
   dbprintf("Readdir %" PRIX64 " (%s) dent=(%" PRIX64 " %s)\n", dent->file_id, grp->path, dent->file_id, dent->name );
   return 1;
}

int detach_cb( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, void* inode_data ) {
   dbprintf("Detach %" PRIX64 " (%s)\n", fent->file_id, grp->path );
   return 0;
}

int main( int argc, char** argv ) {
   
   
   struct fskit_core core;
   int rc;
   void* output;
   
   rc = fskit_test_begin( &core, NULL );
   if( rc != 0 ) {
      exit(1);
   }
   
   // install routes 
   rc = fskit_route_create( &core, "/test-file", create_cb, FSKIT_SEQUENTIAL );
   if( rc != 0 ) {
      errorf("fskit_route_create rc = %d\n", rc );
      exit(1);
   }
   
   rc = fskit_route_mknod( &core, "/test-node", mknod_cb, FSKIT_SEQUENTIAL );
   if( rc != 0 ) {
      errorf("fskit_route_mknod rc = %d\n", rc );
      exit(1);
      
   }
   
   rc = fskit_route_mkdir( &core, "/test-dir", mkdir_cb, FSKIT_SEQUENTIAL );
   if( rc != 0 ) {
      errorf("fskit_route_mkdir rc = %d\n", rc );
      exit(1);
   }
   
   rc = fskit_route_open( &core, "/test-dir", open_cb, FSKIT_SEQUENTIAL );
   if( rc != 0 ) {
      errorf("fskit_route_open rc = %d\n", rc );
      exit(1);
   }
   
   rc = fskit_route_open( &core, "/test-file", open_cb, FSKIT_SEQUENTIAL );
   if( rc != 0 ) {
      errorf("fskit_route_open rc = %d\n", rc );
      exit(1);
   }
   
   rc = fskit_route_close( &core, "/test-file", close_cb, FSKIT_SEQUENTIAL );
   if( rc != 0 ) {
      errorf("fskit_route_close rc = %d\n", rc );
      exit(1);
   }
   
   rc = fskit_route_close( &core, "/test-dir", close_cb, FSKIT_SEQUENTIAL );
   if( rc != 0 ) {
      errorf("fskit_route_close rc = %d\n", rc );
      exit(1);
   }
   
   rc = fskit_route_readdir( &core, "/test-dir", readdir_cb, FSKIT_SEQUENTIAL );
   if( rc != 0 ) {
      errorf("fskit_route_readdir rc = %d\n", rc );
      exit(1);
   }
   
   rc = fskit_route_read( &core, "/test-file", read_cb, FSKIT_SEQUENTIAL );
   if( rc != 0 ) {
      errorf("fskit_route_read rc = %d\n", rc );
      exit(1);
   }
   
   rc = fskit_route_write( &core, "/test-file", write_cb, FSKIT_SEQUENTIAL );
   if( rc != 0 ) {
      errorf("fskit_route_write rc = %d\n", rc );
      exit(1);
   }
   
   rc = fskit_route_trunc( &core, "/test-file", trunc_cb, FSKIT_SEQUENTIAL );
   if( rc != 0 ) {
      errorf("fskit_route_trunc rc = %d\n", rc );
      exit(1);
   }
   
   rc = fskit_route_detach( &core, "/test-file", detach_cb, FSKIT_SEQUENTIAL );
   if( rc != 0 ) {
      errorf("fskit_route_detach rc = %d\n", rc );
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
   
   // mkdir route 
   rc = fskit_mkdir( &core, "/test-dir", 0755, 0, 0 );
   if( rc != 0 ) {
      errorf("fskit_mkdir rc = %d\n", rc );
      exit(1);
   }
   
   // opendir route 
   dh = fskit_opendir( &core, "/test-dir", 0, 0, &rc );
   if( rc != 0 ) {
      errorf("fskit_opendir rc = %d\n", rc );
      exit(1);
      
   }
   
   // readdir route 
   dents = fskit_listdir( &core, dh, &num_dents, &rc );
   if( rc != 0 ) {
      errorf("fskit_listdir rc = %d\n", rc );
      exit(1);
   }
   
   fskit_dir_entry_free_list( dents );
   
   // closedir (close) route 
   rc = fskit_closedir( &core, dh );
   if( rc != 0 ) {
      errorf("fskit_closedir rc = %d\n", rc );
      exit(1);
   }
   
   // create AND truncate route 
   fh = fskit_create( &core, "/test-file", 0, 0, 0644, &rc );
   if( rc != 0 ) {
      errorf("fskit_create rc = %d\n", rc );
      exit(1);
   }
   
   // open route 
   fh2 = fskit_open( &core, "/test-file", 0, 0, O_RDONLY, 0, &rc );
   if( rc != 0 ) {
      errorf("fskit_open rc = %d\n", rc );
      exit(1);
   }
   
   // write route 
   rc = fskit_write( &core, fh, (char*)write_buf, write_len, 0 );
   if( rc != (signed)write_len ) {
      errorf("fskit_write rc = %d\n", rc );
      exit(1);
   }
   
   // read route 
   rc = fskit_read( &core, fh, read_buf, 10, 0 );
   if( rc != (signed)read_len ) {
      errorf("fskit_read rc = %d\n", rc );
      exit(1);
   }
   
   // close route 
   rc = fskit_close( &core, fh );
   if( rc != 0 ) {
      errorf("fskit_close rc = %d\n", rc );
      exit(1);
   }
   
   // close route 
   rc = fskit_close( &core, fh2 );
   if( rc != 0 ) {
      errorf("fskit_close rc = %d\n", rc );
      exit(1);
   }
   
   // mkod route 
   rc = fskit_mknod( &core, "/test-node", S_IFBLK | 0644, makedev( 1, 9 ), 0, 0 );
   if( rc != 0 ) {
      errorf("fskit_mknod rc = %d\n", rc );
      exit(1);
   }
   
   rc = fskit_test_end( &core, &output );
   if( rc != 0 ) {
      errorf("fskit_test_end rc = %d\n", rc );
      exit(1);
   }
   
   return 0;
}
