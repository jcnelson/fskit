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

/*
 * This is a basic fskit-powered tmpfs that counts the number of bytes transferred to/from each file.
 * It exposes the information via xattrs.
 *
 * Usage:
 *    ./fuse-demo [fuse opts] mountpoint_dir
 */

#include "fuse-demo.h"

// fskit inode data 
struct demo_inode {
   char* buf;
   size_t capacity;
   size_t size;
};

// fskit file handle data
struct demo_fd {
   uint64_t num_reads;
   uint64_t num_writes;
};

// file create callback
// create a file descriptor to a new file backed by RAM
int create_cb( struct fskit_core* core, struct fskit_route_metadata* grp, struct fskit_entry* fent, mode_t mode, void** inode_data, void** handle_data ) {

   char buf[17];
   char* path = NULL;
   int rc = 0;
   
   struct demo_inode* di = (struct demo_inode*)calloc( sizeof(struct demo_inode), 1 );

   if( di == NULL ) {
       return -ENOMEM;
   }
   
   struct demo_fd* dfd = (struct demo_fd*)calloc( sizeof(struct demo_fd), 1 );
   
   if( dfd == NULL ) {
       free( di );
       return -ENOMEM;
   }
   
   *inode_data = (void*)di;
   *handle_data = (void*)dfd;
   
   return rc;
}

// file open callback
// create a file descriptor to an existing file backed by RAM
int open_cb( struct fskit_core* core, struct fskit_route_metadata* grp, struct fskit_entry* fent, int flags, void** handle_data ) {

   // only regular files
   if( fskit_entry_get_type( fent ) != FSKIT_ENTRY_TYPE_FILE ) {
      return 0;
   }
   
   int rc = 0;
   struct demo_fd* dfd = (struct demo_fd*)calloc( sizeof(struct demo_fd), 1 );

   if( dfd == NULL ) {
       return -ENOMEM;
   }
   
   *handle_data = dfd;
   return rc;
}

// file close callback
// free the demo_fd
int close_cb( struct fskit_core* core, struct fskit_route_metadata* grp, struct fskit_entry* fent, void* handle_data ) {

   // only regular files
   if( fskit_entry_get_type( fent ) != FSKIT_ENTRY_TYPE_FILE ) {
      return 0;
   }

   struct demo_fd* dfd = (struct demo_fd*)handle_data;

   free( dfd );
   return 0;
}

// file read callback
// copy out of the inode's buffer
int read_cb( struct fskit_core* core, struct fskit_route_metadata* grp, struct fskit_entry* fent, char* buf, size_t buflen, off_t offset, void* handle_data ) {

   struct demo_fd* dfd = (struct demo_fd*)handle_data;
   struct demo_inode* di = (struct demo_inode*)fskit_entry_get_user_data( fent );
   
   int max_read = 0;
   
   if( offset < 0 || (unsigned)offset >= di->size ) {
       // EOF 
       return 0;
   }
   
   max_read = (offset + buflen > di->size ? (di->size - offset) : buflen );
   
   memcpy( buf, di->buf + offset, max_read );
   
   dfd->num_reads += max_read;

   return max_read;
}

// file write callback
// fill in the inode's buffer
int write_cb( struct fskit_core* core, struct fskit_route_metadata* grp, struct fskit_entry* fent, char* buf, size_t buflen, off_t offset, void* handle_data ) {

   struct demo_fd* dfd = (struct demo_fd*)handle_data;
   struct demo_inode* di = (struct demo_inode*)fskit_entry_get_user_data( fent );
   
   if( buflen + offset > di->capacity ) {
       
       if( di->capacity == 0 ) {
           di->capacity = 1;
       }
       
       int exp = (int)(log2( buflen + offset )) + 1;
       di->capacity = 1L << exp;
       
       char* tmp = (char*)realloc( di->buf, di->capacity );
       if( tmp == NULL ) {
           return -ENOMEM;
       }
       
       di->buf = tmp;
   }
   
   memcpy( di->buf, buf, buflen );
   
   dfd->num_writes += buflen;
   
   di->size = (di->size > buflen + offset ? di->size : buflen + offset);

   return (int)buflen;
}

// file destroy callback
// free memory 
int destroy_cb( struct fskit_core* core, struct fskit_route_metadata* grp, struct fskit_entry* fent, void* inode_data ) {

   struct demo_inode* di = (struct demo_inode*)fskit_entry_get_user_data( fent );
   
   if( di == NULL ) {
       return 0;
   }
   
   if( di->buf != NULL ) {
      free( di->buf );
   }
   
   free( di );

   return 0;
}

void usage( char const* progname ) {
   fprintf(stderr, "Usage: %s [FUSE options] MOUNTPOINT\n", progname);
}

int main( int argc, char** argv ) {

   int rc = 0;
   struct fskit_fuse_state* state = NULL;
   struct fskit_core* core = NULL;

   if( argc < 2 ) {
      usage( argv[0] );
      exit(1);
   }

   state = fskit_fuse_state_new();
   if( state == NULL ) {
      fprintf(stderr, "Out of memory\n");
      exit(1);
   }

   fskit_set_debug_level( 1 );
   fskit_set_error_level( 1 );
   
   // set up
   rc = fskit_fuse_init( state, NULL );
   if( rc != 0 ) {
      fprintf(stderr, "fskit_fuse_init rc = %d\n", rc );
      exit(1);
   }

   core = fskit_fuse_get_core( state );

   // add handlers.  reads and writes must happen sequentially, since we seek and then perform I/O
   // NOTE: FSKIT_ROUTE_ANY matches any path, and is a macro for the regex "/([^/]+[/]*)*"
   fskit_route_create( core, FSKIT_ROUTE_ANY, create_cb,  FSKIT_CONCURRENT );
   fskit_route_open(   core, FSKIT_ROUTE_ANY, open_cb,    FSKIT_CONCURRENT );
   fskit_route_read(   core, FSKIT_ROUTE_ANY, read_cb,    FSKIT_SEQUENTIAL );
   fskit_route_write(  core, FSKIT_ROUTE_ANY, write_cb,   FSKIT_SEQUENTIAL );
   fskit_route_close(  core, FSKIT_ROUTE_ANY, close_cb,   FSKIT_CONCURRENT );
   fskit_route_destroy(core, FSKIT_ROUTE_ANY, destroy_cb, FSKIT_CONCURRENT );

   // set the root to be owned by the effective UID and GID of user
   fskit_chown( core, "/", 0, 0, geteuid(), getegid() );
   fskit_chmod( core, "/", 0, 0, 0755 );

   // run
   rc = fskit_fuse_main( state, argc, argv );

   // shutdown
   fskit_fuse_shutdown( state, NULL );
   free( state );

   return rc;
}
