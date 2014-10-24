/*
   fuse-demo: a FUSE filesystem demo of fskit
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

/*
 * This is a basic fskit-powered FUSE filesystem that stores data to disk.
 * Instead of storing data by path, it stores data by inode number.
 * 
 * Usage:
 *    ./fuse-demo [fuse opts] storage_dir mountpoint_dir
 */

#include "fuse-demo.h"

// fskit file handle data
struct demo_fd {
   int fd;
};

// convert an inode to a 16-byte null-terminated string 
int inode_to_string( uint64_t inode_num, char* buf ) {
      
   memset( buf, 0, 17 );
   sprintf( buf, "%016" PRIX64, inode_num );
   return 0;
}

// file create callback
// create the file $storage_dir/$inode
// keep the file descriptor in a demo_fd.
int create_cb( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, mode_t mode, void** inode_data, void** handle_data ) {
   
   char buf[17];
   char* path = NULL;
   char* storage_root = (char*)fskit_core_get_user_data( core );
   int rc = 0;
   struct demo_fd* dfd = (struct demo_fd*)calloc( sizeof(struct demo_fd), 1 );
   
   inode_to_string( fent->file_id, buf );
   path = fskit_fullpath( storage_root, buf, NULL );
   
   dfd->fd = creat( path, mode );
   rc = -errno;
   
   free( path );
   
   if( dfd->fd >= 0 ) {
      *handle_data = (void*)dfd;
      return 0;
   }
   else {
      free( dfd );
      return rc;
   }
}

// file open callback 
// open the file $storage_dir/$inode 
// keep the file descriptor in a demo_fd
int open_cb( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, int flags, void** handle_data ) {
   
   char buf[17];
   char* path = NULL;
   char* storage_root = (char*)fskit_core_get_user_data( core );
   int rc = 0;
   uint64_t inode_num = fskit_entry_get_file_id( fent );
   struct demo_fd* dfd = (struct demo_fd*)calloc( sizeof(struct demo_fd), 1 );
   
   inode_to_string( inode_num, buf );
   path = fskit_fullpath( storage_root, buf, NULL );
   
   dfd->fd = open( path, flags );
   rc = -errno;
   
   free( path );
   
   if( dfd->fd >= 0 ) {
      *handle_data = (void*)dfd;
      return 0;
   }
   else {
      free( dfd );
      return rc;
   }
}

// file close callback
// close the file $storage_dir/$inode
// free the demo_fd
int close_cb( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, void* handle_data ) {
   
   struct demo_fd* dfd = (struct demo_fd*)handle_data;
   int rc = 0;
   
   close( dfd->fd );
   rc = -errno;
   free( dfd );
   return rc;
}

// file read callback 
// extract the actual file descriptor and call read(2)
int read_cb( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, char* buf, size_t buflen, off_t offset, void* handle_data ) {
   
   struct demo_fd* dfd = (struct demo_fd*)handle_data;
   lseek( dfd->fd, offset, SEEK_SET );
   
   return read( dfd->fd, buf, buflen );
}

// file write callback
// extract the actual file descriptor and call write(2)
int write_cb( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, char* buf, size_t buflen, off_t offset, void* handle_data ) {
   
   struct demo_fd* dfd = (struct demo_fd*)handle_data;
   lseek( dfd->fd, offset, SEEK_SET );
   
   return write( dfd->fd, buf, buflen );
}

// file unlink callback 
// unlink $storage_dir/$inode
int detach_cb( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, void* inode_data ) {
   
   char buf[17];
   char* path = NULL;
   int rc = 0;
   char* storage_root = (char*)fskit_core_get_user_data( core );
   uint64_t inode_num = fskit_entry_get_file_id( fent );
   
   inode_to_string( inode_num, buf );
   path = fskit_fullpath( storage_root, buf, NULL );
   
   unlink( path );
   rc = -errno;
   
   free( path );
   return rc;
}

void usage( char const* progname ) {
   fprintf(stderr, "Usage: %s [FUSE options] STORAGE MOUNTPOINT\n", progname);
}

int main( int argc, char** argv ) {
   
   int rc = 0;
   struct fskit_fuse_state state;
   struct fskit_core* core = NULL;
   char* storage_dir = NULL;
   
   if( argc < 3 ) {
      usage( argv[0] );
      exit(1);
   }
   
   storage_dir = argv[ argc - 2 ];
   argv[ argc - 2 ] = argv[ argc - 1 ];
   argv[ argc - 1 ] = NULL;
   argc--;
   
   // set up 
   rc = fskit_fuse_init( &state, storage_dir );
   if( rc != 0 ) {
      fprintf(stderr, "fskit_fuse_init rc = %d\n", rc );
      exit(1);
   }
   
   core = fskit_fuse_get_core( &state );
   
   // add handlers.  reads and writes must happen sequentially, since we seek and then perform I/O
   // NOTE: FSKIT_ROUTE_ANY matches any path, and is a macro for the regex "/([^/]+[/]*)+"
   fskit_route_create( core, FSKIT_ROUTE_ANY, create_cb, FSKIT_CONCURRENT );
   fskit_route_open(   core, FSKIT_ROUTE_ANY, open_cb,   FSKIT_CONCURRENT );
   fskit_route_read(   core, FSKIT_ROUTE_ANY, read_cb,   FSKIT_SEQUENTIAL );
   fskit_route_write(  core, FSKIT_ROUTE_ANY, write_cb,  FSKIT_SEQUENTIAL );
   fskit_route_close(  core, FSKIT_ROUTE_ANY, close_cb,  FSKIT_CONCURRENT );
   fskit_route_detach( core, FSKIT_ROUTE_ANY, detach_cb, FSKIT_CONCURRENT );
   
   // set the root to be owned by the effective UID and GID of user
   fskit_chown( core, "/", 0, 0, geteuid(), getegid() );
   
   // run 
   rc = fskit_fuse_main( &state, argc, argv );
   
   // shutdown
   fskit_fuse_shutdown( &state, NULL );
   
   return rc;
}
