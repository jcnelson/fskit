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

#include "test-mknod.h"

int main( int argc, char** argv ) {
   
   struct fskit_core core;
   int rc;
   char name_buf[10];
   void* output;
   
   rc = fskit_test_begin( &core, NULL );
   if( rc != 0 ) {
      exit(1);
   }
   
   int num_types = 5;
   int types[] = {
      S_IFREG,
      S_IFIFO,
      S_IFSOCK,
      S_IFCHR,
      S_IFBLK
   };
   
   for( int i = 0; i < 256; i++ ) {
      
      sprintf(name_buf, "/%d", i );
      
      int type = types[ i % num_types ];
      
      int rc = fskit_mknod( &core, name_buf, type | 0644, makedev( i, i ), 0, i );
      
      if( rc != 0 ) {
         fskit_error("fskit_mknod( path=%s, type=%o ) rc = %d\n", name_buf, type, rc );
         exit(1);
      }
   }
   
   fskit_print_tree( stdout, &core.root );
   
   fskit_test_end( &core, &output );
   
   return 0;
}