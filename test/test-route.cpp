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
   fskit_debug("Create %" PRIX64 " (%s) mode=%o\n", fskit_entry_get_file_id( fent ), fskit_route_metadata_get_path( route_metadata ), mode );
   return 0;
}

int mknod_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, mode_t mode, dev_t dev, void** inode_data ) {
   fskit_debug("Mknod %" PRIX64 " (%s) mode=%o dev=%lX \n", fskit_entry_get_file_id( fent ), fskit_route_metadata_get_path( route_metadata ), mode, dev );
   return 0;
}

int mkdir_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* dent, mode_t mode, void** inode_data ) {
   fskit_debug("Mkdir %" PRIX64 " (%s) mode=%o\n", fskit_entry_get_file_id( dent ), fskit_route_metadata_get_path( route_metadata ), mode );
   return 0;
}

int open_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, int flags, void** handle_data ) {
   fskit_debug("Open %" PRIX64 " (%s) flags=%X\n", fskit_entry_get_file_id( fent ), fskit_route_metadata_get_path( route_metadata ), flags );
   return 0;
}

int close_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, void* handle_data ) {
   fskit_debug("Close %" PRIX64 " (%s)\n", fskit_entry_get_file_id( fent ), fskit_route_metadata_get_path( route_metadata ) );
   return 0;
}

int read_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, char* buf, size_t buflen, off_t offset, void* handle_data ) {
   fskit_debug("Read %" PRIX64 " (%s) buf=%p buflen=%zu offset=%jd handle_data=%p\n", fskit_entry_get_file_id( fent ), fskit_route_metadata_get_path( route_metadata ), buf, buflen, offset, handle_data );
   return buflen;
}

int write_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, char* buf, size_t buflen, off_t offset, void* handle_data ) {
   fskit_debug("Write %" PRIX64 " (%s) buf=%p buflen=%zu offset=%jd handle_data=%p\n", fskit_entry_get_file_id( fent ), fskit_route_metadata_get_path( route_metadata ), buf, buflen, offset, handle_data );
   return buflen;
}

int trunc_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, off_t new_size, void* handle_data ) {
   fskit_debug("Trunc %" PRIX64 " (%s) new_size=%jd handle_data=%p\n", fskit_entry_get_file_id( fent ), fskit_route_metadata_get_path( route_metadata ), new_size, handle_data );
   return 0;
}

int readdir_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* dir, struct fskit_dir_entry** dents, size_t num_dents ) {
   fskit_debug("Readdir %" PRIX64 " (%s) dents[0]=(%" PRIX64 " %s), num_dents=%" PRIu64 "\n", dents[0]->file_id, fskit_route_metadata_get_path( route_metadata ), dents[0]->file_id, dents[0]->name, num_dents );
   return 0;
}

int detach_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, void* inode_data ) {
   fskit_debug("Detach %" PRIX64 " (%s)\n", fskit_entry_get_file_id( fent ), fskit_route_metadata_get_path( route_metadata ) );
   return 0;
}

int stat_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, struct stat* sb ) {
   fskit_debug("Stat %" PRIX64 " (%s)\n", fskit_entry_get_file_id( fent ), fskit_route_metadata_get_path( route_metadata ) );
   return 0;
}

int sync_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent ) {
   fskit_debug("Sync %" PRIX64 " (%s)\n", fskit_entry_get_file_id( fent ), fskit_route_metadata_get_path( route_metadata ) );
   return 0;
}

int rename_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, char const* new_path, struct fskit_entry* new_fent ) {
   fskit_debug("Rename %" PRIX64 " from %s to %s\n", fskit_entry_get_file_id( fent ), fskit_route_metadata_get_path( route_metadata ), new_path );
   return 0;
}

int link_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, char const* new_path ) {
   fskit_debug("Link %" PRIX64 " to %s\n", fskit_entry_get_file_id( fent ), fskit_route_metadata_get_path( route_metadata ), new_path );
   return 0;
}

int getxattr_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, char const* xattr_name, char* xattr_buf, size_t xattr_buf_len ) {
   fskit_debug("Getxattr %" PRIX64 ".%s\n", fskit_entry_get_file_id( fent ), fskit_route_metadata_get_xattr_name( route_metadata ) );
   return xattr_buf_len;
}

int setxattr_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, char const* xattr_name, char const* xattr_value, size_t xattr_value_len, int flags ) {
   fskit_debug("Setxattr %" PRIX64 ".%s (%X)\n", fskit_entry_get_file_id( fent ), fskit_route_metadata_get_xattr_name( route_metadata ), flags );
   return 0;
}

int listxattr_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, char* xattr_buf, size_t xattr_buf_len ) {
   fskit_debug("Listxattr %" PRIX64 "\n", fskit_entry_get_file_id( fent ) );
   return xattr_buf_len;
}

int removexattr_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, char const* xattr_name ) {
   fskit_debug("Removexattr %" PRIX64 ".%s\n", fskit_entry_get_file_id( fent ), xattr_name );
   return 0;
}

int setmetadata_cb( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, struct fskit_inode_metadata* imd ) {
   fskit_debug("set inode metadata: %X (%o, %" PRIu64 ", %" PRIu64 ")\n", fskit_inode_metadata_get_inventory(imd), fskit_inode_metadata_get_mode(imd), fskit_inode_metadata_get_owner(imd), fskit_inode_metadata_get_group(imd));
   return 0;
}

int main( int argc, char** argv ) {
   struct fskit_core* core = NULL;
   int rc;
   void* output;
   
   int create_rh, mknod_rh, mkdir_rh, opendir_rh, open_rh, close_rh, 
       closedir_rh, readdir_rh, read_rh, write_rh, trunc_rh, unlink_rh, 
       rmdir_rh, stat_rh, sync_rh, rename_rh, link_rh, getxattr_rh, listxattr_rh,
       removexattr_rh, setxattr_rh, setmetadata_rh;

   rc = fskit_test_begin( &core, NULL );
   if( rc != 0 ) {
      exit(1);
   }

   // install routes
   create_rh = fskit_route_create( core, "/test-file", create_cb, FSKIT_SEQUENTIAL );
   if( create_rh < 0 ) {
      fskit_error("fskit_route_create rc = %d\n", create_rh );
      exit(1);
   }

   mknod_rh = fskit_route_mknod( core, "/test-node", mknod_cb, FSKIT_SEQUENTIAL );
   if( mknod_rh < 0 ) {
      fskit_error("fskit_route_mknod rc = %d\n", mknod_rh );
      exit(1);

   }

   mkdir_rh = fskit_route_mkdir( core, "/test-dir", mkdir_cb, FSKIT_SEQUENTIAL );
   if( mkdir_rh < 0 ) {
      fskit_error("fskit_route_mkdir rc = %d\n", mkdir_rh );
      exit(1);
   }

   opendir_rh = fskit_route_open( core, "/test-dir", open_cb, FSKIT_SEQUENTIAL );
   if( opendir_rh < 0 ) {
      fskit_error("fskit_route_open rc = %d\n", opendir_rh );
      exit(1);
   }

   open_rh = fskit_route_open( core, "/test-file", open_cb, FSKIT_SEQUENTIAL );
   if( open_rh < 0 ) {
      fskit_error("fskit_route_open rc = %d\n", open_rh );
      exit(1);
   }

   close_rh = fskit_route_close( core, "/test-file", close_cb, FSKIT_SEQUENTIAL );
   if( close_rh < 0 ) {
      fskit_error("fskit_route_close rc = %d\n", close_rh );
      exit(1);
   }

   closedir_rh = fskit_route_close( core, "/test-dir", close_cb, FSKIT_SEQUENTIAL );
   if( closedir_rh < 0 ) {
      fskit_error("fskit_route_close rc = %d\n", closedir_rh );
      exit(1);
   }

   readdir_rh = fskit_route_readdir( core, "/test-dir", readdir_cb, FSKIT_SEQUENTIAL );
   if( readdir_rh < 0 ) {
      fskit_error("fskit_route_readdir rc = %d\n", readdir_rh );
      exit(1);
   }

   read_rh = fskit_route_read( core, "/test-file", read_cb, FSKIT_SEQUENTIAL );
   if( read_rh < 0 ) {
      fskit_error("fskit_route_read rc = %d\n", read_rh );
      exit(1);
   }

   write_rh = fskit_route_write( core, "/test-file", write_cb, FSKIT_SEQUENTIAL );
   if( write_rh < 0 ) {
      fskit_error("fskit_route_write rc = %d\n", write_rh );
      exit(1);
   }

   trunc_rh = fskit_route_trunc( core, "/test-file", trunc_cb, FSKIT_SEQUENTIAL );
   if( trunc_rh < 0 ) {
      fskit_error("fskit_route_trunc rc = %d\n", trunc_rh );
      exit(1);
   }

   unlink_rh = fskit_route_detach( core, "/test-file", detach_cb, FSKIT_SEQUENTIAL );
   if( unlink_rh < 0 ) {
      fskit_error("fskit_route_detach rc = %d\n", unlink_rh );
      exit(1);
   }

   rmdir_rh = fskit_route_detach( core, "/test-dir", detach_cb, FSKIT_SEQUENTIAL );
   if( rmdir_rh < 0 ) {
      fskit_error("fskit_route_detach rc = %d\n", rmdir_rh );
      exit(1);
   }

   stat_rh = fskit_route_stat( core, "/test-file", stat_cb, FSKIT_SEQUENTIAL );
   if( stat_rh < 0 ) {
      fskit_error("fskit_route_stat rc = %d\n", stat_rh );
      exit(1);
   }

   sync_rh = fskit_route_sync( core, "/test-file", sync_cb, FSKIT_SEQUENTIAL );
   if( sync_rh < 0 ) {
      fskit_error("fskit_route_sync rc = %d\n", sync_rh );
      exit(1);
   }
   
   rename_rh = fskit_route_rename( core, "/test-file", rename_cb, FSKIT_SEQUENTIAL );
   if( rename_rh < 0 ) {
      fskit_error("fskit_route_rename rc = %d\n", rename_rh );
      exit(1);
   }

   link_rh = fskit_route_link( core, "/test-file", link_cb, FSKIT_SEQUENTIAL );
   if( link_rh < 0 ) {
      fskit_error("fskit_route_link rc = %d\n", link_rh );
      exit(1);
   }

   getxattr_rh = fskit_route_getxattr( core, "/test-file", getxattr_cb, FSKIT_SEQUENTIAL );
   if( getxattr_rh < 0 ) {
      fskit_error("fskit_route_getxattr rc = %d\n", getxattr_rh );
      exit(1);
   }

   listxattr_rh = fskit_route_listxattr( core, "/test-file", listxattr_cb, FSKIT_SEQUENTIAL );
   if( listxattr_rh < 0 ) {
      fskit_error("fskit_route_listxattr rc = %d\n", listxattr_rh );
      exit(1);
   }

   removexattr_rh = fskit_route_removexattr( core, "/test-file", removexattr_cb, FSKIT_SEQUENTIAL );
   if( removexattr_rh < 0 ) {
      fskit_error("fskit_route_removexattr rc = %d\n", removexattr_rh );
      exit(1);
   }

   setxattr_rh = fskit_route_setxattr( core, "/test-file", setxattr_cb, FSKIT_SEQUENTIAL );
   if( setxattr_rh < 0 ) {
      fskit_error("fskit_route_setxattr rc = %d\n", setxattr_rh );
      exit(1);
   }

   setmetadata_rh = fskit_route_setmetadata( core, "/test-file", setmetadata_cb, FSKIT_INODE_SEQUENTIAL );
   if( setmetadata_rh < 0 ) {
      fskit_error("fakit_route_setmetadata rc = %d\n", setmetadata_rh );
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
   rc = fskit_mkdir( core, "/test-dir", 0755, 0, 0 );
   if( rc != 0 ) {
      fskit_error("fskit_mkdir rc = %d\n", rc );
      exit(1);
   }

   // opendir route
   dh = fskit_opendir( core, "/test-dir", 0, 0, &rc );
   if( rc != 0 ) {
      fskit_error("fskit_opendir rc = %d\n", rc );
      exit(1);

   }

   // readdir route
   dents = fskit_listdir( core, dh, &num_dents, &rc );
   if( rc != 0 ) {
      fskit_error("fskit_listdir rc = %d\n", rc );
      exit(1);
   }

   fskit_dir_entry_free_list( dents );

   // closedir (close) route
   rc = fskit_closedir( core, dh );
   if( rc != 0 ) {
      fskit_error("fskit_closedir rc = %d\n", rc );
      exit(1);
   }

   // create AND truncate route
   fh = fskit_create( core, "/test-file", 0, 0, 0644, &rc );
   if( rc != 0 ) {
      fskit_error("fskit_create rc = %d\n", rc );
      exit(1);
   }

   // open route
   fh2 = fskit_open( core, "/test-file", 0, 0, O_RDONLY, 0, &rc );
   if( rc != 0 ) {
      fskit_error("fskit_open rc = %d\n", rc );
      exit(1);
   }

   // write route
   rc = fskit_write( core, fh, (char*)write_buf, write_len, 0 );
   if( rc != (signed)write_len ) {
      fskit_error("fskit_write rc = %d\n", rc );
      exit(1);
   }

   // read route
   rc = fskit_read( core, fh2, read_buf, 10, 0 );
   if( rc != (signed)read_len ) {
      fskit_error("fskit_read rc = %d\n", rc );
      exit(1);
   }

   // sync route
   rc = fskit_fsync( core, fh );
   if( rc != 0 ) {
      fskit_error("fskit_sync rc = %d\n", rc );
      exit(1);
   }

   // close route
   rc = fskit_close( core, fh );
   if( rc != 0 ) {
      fskit_error("fskit_close rc = %d\n", rc );
      exit(1);
   }

   // close route
   rc = fskit_close( core, fh2 );
   if( rc != 0 ) {
      fskit_error("fskit_close rc = %d\n", rc );
      exit(1);
   }

   // mkod route
   rc = fskit_mknod( core, "/test-node", S_IFBLK | 0644, makedev( 1, 9 ), 0, 0 );
   if( rc != 0 ) {
      fskit_error("fskit_mknod rc = %d\n", rc );
      exit(1);
   }

   // stat route
   rc = fskit_stat( core, "/test-file", 0, 0, &sb );
   if( rc != 0 ) {
      fskit_error("fskit_stat rc = %d\n", rc );
      exit(1);
   }
   
   // rename 
   rc = fskit_rename( core, "/test-file", "/test-file-renamed", 0, 0 );
   if( rc != 0 ) {
      fskit_error("fskit_rename rc = %d\n", rc );
      exit(1);
   }
   
   rc = fskit_rename( core, "/test-file-renamed", "/test-file", 0, 0 );
   if( rc != 0 ) {
      fskit_error("fskit_rename rc = %d\n", rc );
      exit(1);
   }

   // link 
   rc = fskit_link( core, "/test-file", "/test-file-linked", 0, 0 );
   if( rc != 0 ) {
      fskit_error("fskit_link rc = %d\n", rc );
      exit(1);
   }

   // setxattr 
   rc = fskit_setxattr( core, "/test-file", 0, 0, "foo", "bar", 4, XATTR_CREATE );
   if( rc < 0 ) {
      fskit_error("fskit_setxattr rc = %d\n", rc );
      exit(1);
   }

   // listxattr 
   char attrbuf[100];
   rc = fskit_listxattr( core, "/test-file", 0, 0, attrbuf, 100 );
   if( rc < 0 ) {
      fskit_error("fskit_listxattr rc = %d\n", rc );
      exit(1);
   }

   // getxattr
   rc = fskit_getxattr( core, "/test-file", 0, 0, "foo", attrbuf, 100 );
   if( rc < 0 ) {
      fskit_error("fskit_getxattr rc = %d\n", rc );
      exit(1);
   }

   // removexattr 
   rc = fskit_removexattr( core, "/test-file", 0, 0, "foo" );
   if( rc < 0 ) {
      fskit_error("fskti_removexattr rc = %d\n", rc );
      exit(1);
   }

   // chown 
   rc = fskit_chown( core, "/test-file", 0, 0, 123, 456 );
   if( rc < 0 ) {
      fskit_error("fskit_chown rc = %d\n", rc );
      exit(1);
   }

   // chmod 
   rc = fskit_chmod( core, "/test-file", 123, 456, 0765 );
   if( rc < 0 ) {
      fskit_error("fskit_chmod rc = %d\n", rc );
      exit(1);
   }

   // chown 
   rc = fskit_chown( core, "/test-file", 123, 456, 0, 0 );
   if( rc < 0 ) {
      fskit_error("fskit_chown rc = %d\n", rc );
      exit(1);
   }

   // chmod 
   rc = fskit_chmod( core, "/test-file", 0, 0, 0555 );
   if( rc < 0 ) {
      fskit_error("fskit_chmod rc = %d\n", rc );
      exit(1);
   }

   // undefine routes
   rc = fskit_unroute_create( core, create_rh );
   if( rc != 0 ) {
      fskit_error("fskit_unroute_create rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_unroute_mknod( core, mknod_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_mknod rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_unroute_mkdir( core, mkdir_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_mkdir rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_unroute_open( core, open_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_open rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_unroute_open( core, opendir_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_open rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_unroute_close( core, close_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_close rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_unroute_close( core, closedir_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_close rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_unroute_readdir( core, readdir_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_readdir rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_unroute_read( core, read_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_read rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_unroute_write( core, write_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_write rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_unroute_trunc( core, trunc_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_trunc rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_unroute_detach( core, unlink_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_detach rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_unroute_detach( core, rmdir_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_detach rc = %d\n", rc );
      exit(1);
   }
   
   rc = fskit_unroute_rename( core, rename_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_rename rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_unroute_link( core, link_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_link rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_unroute_getxattr( core, getxattr_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_getxattr rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_unroute_listxattr( core, listxattr_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_listxattr rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_unroute_removexattr( core, removexattr_rh );
   if( rc < 0 ) {
      fskit_error("fskit_unroute_removexattr rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_test_end( core, &output );
   if( rc != 0 ) {
      fskit_error("fskit_test_end rc = %d\n", rc );
      exit(1);
   }
   return 0;
}
