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

#include "test-rmdir.h"

int fskit_test_rmdir_recursive( struct fskit_core* core, char const* path ) {

   int rc = 0;
   struct fskit_dir_entry** dents;
   uint64_t num_read = 0;

   struct fskit_dir_handle* dh = fskit_opendir( core, path, 0, 0, &rc );

   if( rc != 0 ) {
      fskit_error("fskit_opendir('%s') rc = %d\n", path, rc );
      return rc;
   }

   dents = fskit_listdir( core, dh, &num_read, &rc );
   if( rc != 0 ) {
      fskit_error("fskit_listdir('%s') rc = %d\n", path, rc );
      return rc;
   }

   if( num_read == 0 ) {

      // nothing to do
      rc = fskit_closedir( core, dh );
      if( rc != 0 ) {
         fskit_error("fskit_closedir('%s') rc = %d\n", path, rc );
      }

      return rc;
   }

   for( uint64_t i = 0; i < num_read; i++ ) {

      if( strcmp(dents[i]->name, ".") == 0 || strcmp(dents[i]->name, "..") == 0 ) {
         continue;
      }

      if( dents[i]->type != FSKIT_ENTRY_TYPE_DIR ) {
         fskit_error("ERR: directory '%s' not empty\n", path );
         rc = -ENOTEMPTY;
         break;
      }

      char* child_path = fskit_fullpath( path, dents[i]->name, NULL );

      rc = fskit_test_rmdir_recursive( core, child_path );

      if( rc != 0 ) {
         fskit_error("fskit_test_rmdir_recursive('%s') rc = %d\n", child_path, rc );

         free( child_path );
         break;
      }

      free( child_path );
   }

   fskit_dir_entry_free_list( dents );

   if( rc == 0 ) {
      // remove this directory
      fskit_debug("rmdir '%s'\n", path );
      rc = fskit_rmdir( core, path, 0, 0 );
      if( rc != 0 ) {
         fskit_error("fskit_rmdir('%s') rc = %d\n", path, rc );
      }
   }

   rc = fskit_closedir( core, dh );
   if( rc != 0 ) {
      fskit_error("fskit_closedir('%s') rc = %d\n", path, rc );
   }

   return rc;
}

int main( int argc, char** argv ) {

   struct fskit_core* core = NULL;
   int rc;
   void* output;

   rc = fskit_test_begin( &core, NULL );
   if( rc != 0 ) {
      exit(1);
   }

   rc = fskit_test_mkdir_LR_recursive( core, "/root", 7 );
   if( rc != 0 ) {
      fskit_error("fskit_test_mkdir_LR_recursive('/root') rc = %d\n", rc );
      exit(1);
   }

   rc = fskit_test_rmdir_recursive( core, "/root" );
   if( rc != 0 ) {
      fskit_error("fskit_test_rmdir_recursive('/root') rc = %d\n", rc );
      exit(1);
   }

   fskit_print_tree( stdout, fskit_core_get_root( core ) );

   fskit_test_end( core, &output );

   return 0;
}
