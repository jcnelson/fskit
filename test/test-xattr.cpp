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


#include "test-xattr.h"

// list and print all xattrs 
static int print_xattrs( struct fskit_core* core, char const* path_buf ) {
   
   int rc = 0;
   char name_buf[100];
   char xattr_buf[100];
   
   // list xattr 
   char listxattr_buf[4096];
   memset( listxattr_buf, 0, 4096 );
   
   int len = fskit_listxattr( core, path_buf, 0, 0, listxattr_buf, 4095 );
   if( len < 0 ) {
      fskit_error("fskit_listxattr rc = %d\n", len );
      return rc;
   }
   
   printf("listxattr: %d bytes returned\n", len );
   
   // get xattr
   for( int i = 0; i < len; ) {
      
      memset( name_buf, 0, 100 );
      memset( xattr_buf, 0, 100 );
      memset( name_buf, 0, 100 );
      
      sprintf(name_buf, listxattr_buf + i);
      
      rc = fskit_getxattr( core, path_buf, 0, 0, name_buf, xattr_buf, 100 );
      if( rc < 0 ) {
         fskit_error("fskit_getxattr( '%s', '%s' ) rc = %d\n", path_buf, name_buf, rc );
         return rc;
      }
      
      printf("attr: '%s' = '%s'\n", name_buf, xattr_buf );
      
      i += rc;
   }
   
   return 0;
}
   


int main( int argc, char** argv ) {
   
   struct fskit_core core;
   int rc;
   void* output = NULL;
   char path_buf[100];
   char name_buf[100];
   char xattr_buf[100];
   struct fskit_file_handle* fh = NULL;
   
   rc = fskit_test_begin( &core, NULL );
   if( rc != 0 ) {
      exit(1);
   }
   
   // make an entry 
   sprintf(path_buf, "/test" );
   
   fh = fskit_create( &core, path_buf, 0, 0, 0644, &rc );
   
   if( fh == NULL ) {
      fskit_error("fskit_create('%s') rc = %d\n", path_buf, rc );
      exit(1);
   }
   
   fskit_close( &core, fh );
   
   // set xattr 
   for( int i = 0; i < 10; i++ ) {
      
      memset( xattr_buf, 0, 100 );
      memset( name_buf, 0, 100 );
      
      sprintf(name_buf, "attr-name-%d", i );
      sprintf(xattr_buf, "attr-value-%d", i );
      
      rc = fskit_setxattr( &core, path_buf, 0, 0, name_buf, xattr_buf, strlen(xattr_buf), XATTR_CREATE );
      if( rc != 0 ) {
         fskit_error("fskit_setxattr( '%s', '%s', '%s' ) rc = %d\n", path_buf, name_buf, xattr_buf, rc );
         exit(1);
      }
   }
   
   // list xattr and getxattr
   rc = print_xattrs( &core, path_buf );
   if( rc != 0 ) {
      exit(1);
   }
   
   // remove xattr 
   for( int i = 0; i < 10; i++ ) {
      
      memset( name_buf, 0, 100 );
      
      sprintf(name_buf, "attr-name-%d", i );
      
      rc = fskit_removexattr( &core, path_buf, 0, 0, name_buf );
      if( rc != 0 ) {
         fskit_error("fskit_removexattr( '%s', '%s' ) rc = %d\n", path_buf, name_buf, rc );
         exit(1);
      }
   }
   
   rc = print_xattrs( &core, path_buf );
   if( rc != 0 ) {
      exit(1);
   }
   
   fskit_test_end( &core, &output );
   
   return 0;
}