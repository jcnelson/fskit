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

#include "common.h"

int main( int argc, char** argv ) {

   struct fskit_core* core = NULL;
   int rc;
   void* output;
   struct fskit_path_iterator* itr = NULL;
   struct fskit_file_handle* fh = NULL;

   rc = fskit_test_begin( &core, NULL );
   if( rc != 0 ) {
      exit(1);
   }

   rc = fskit_test_mkdir_LR_recursive( core, "/root", 7 );
   if( rc != 0 ) {
      fskit_error("fskit_test_mkdir_LR_recursive('/root') rc = %d\n", rc );
      exit(1);
   }
   
   fh = fskit_create( core, "/root/L/R/L/R/L/R/.foo", 0, 0, 0777, &rc );
   if( fh == NULL ) {
      fskit_error("fskit_create('/root/L/R/L/R/L/R/.foo') rc = %d\n", rc );
      exit(1);
   }
   
   fskit_close( core, fh );
   
   fh = fskit_create( core, "/bar.f", 0, 0, 0777, &rc );
   if( fh == NULL ) {
      fskit_error("fskit_create('/bar.f') rc = %d\n", rc );
      exit(1);
   }
   
   fskit_close( core, fh );
   
   rc = fskit_mkdir( core, "/bar.d", 0755, 0, 0 );
   if( rc < 0 ) {
      fskit_error("fskit_mkdir('/bar.d') rc = %d\n", rc );
      exit(1);  
   }
   
   /////////////////////////////////////////////////////////////////////////////////////
   printf("Iterate succeeds...\n");
   
   for( itr = fskit_path_begin( core, "/root/L/R/L/R/L/R/.foo", true ); !fskit_path_end( itr ); fskit_path_next( itr ) ) {
      
      struct fskit_entry* cur = fskit_path_iterator_entry( itr );
      char* cur_path = fskit_path_iterator_path( itr );
      
      printf("Entry %016" PRIX64 " (%p): %c %s\n", fskit_entry_get_file_id( cur ), cur, fskit_entry_get_type( cur ) == FSKIT_ENTRY_TYPE_FILE ? 'F' : 'D', cur_path );
      
      free( cur_path );
   }
   
   printf("Iterator error: %d\n", fskit_path_iterator_error( itr ) );
   fskit_path_iterator_release( itr );
   
   for( itr = fskit_path_begin( core, "/bar.f", true ); !fskit_path_end( itr ); fskit_path_next( itr ) ) {
      
      struct fskit_entry* cur = fskit_path_iterator_entry( itr );
      char* cur_path = fskit_path_iterator_path( itr );
      
      printf("Entry %016" PRIX64 " (%p): %c %s\n", fskit_entry_get_file_id( cur ), cur, fskit_entry_get_type( cur ) == FSKIT_ENTRY_TYPE_FILE ? 'F' : 'D', cur_path );
      
      free( cur_path );
   }
   
   printf("Iterator error: %d\n", fskit_path_iterator_error( itr ) );
   fskit_path_iterator_release( itr );
   
   
   for( itr = fskit_path_begin( core, "/bar.d", true ); !fskit_path_end( itr ); fskit_path_next( itr ) ) {
      
      struct fskit_entry* cur = fskit_path_iterator_entry( itr );
      char* cur_path = fskit_path_iterator_path( itr );
      
      printf("Entry %016" PRIX64 " (%p): %c %s\n", fskit_entry_get_file_id( cur ), cur, fskit_entry_get_type( cur ) == FSKIT_ENTRY_TYPE_FILE ? 'F' : 'D', cur_path );
      
      free( cur_path );
   }
   
   printf("Iterator error: %d\n", fskit_path_iterator_error( itr ) );
   fskit_path_iterator_release( itr );
   
   
   /////////////////////////////////////////////////////////////////////////////////////
   printf("Iterate succeeds on path with duplicate . and /...\n");
   
   for( itr = fskit_path_begin( core, "././root/L/R//L//././/R/L//.///R", true ); !fskit_path_end( itr ); fskit_path_next( itr ) ) {
      
      struct fskit_entry* cur = fskit_path_iterator_entry( itr );
      char* cur_path = fskit_path_iterator_path( itr );
      
      printf("Entry %016" PRIX64 " (%p): %c %s\n", fskit_entry_get_file_id( cur ), cur, fskit_entry_get_type( cur ) == FSKIT_ENTRY_TYPE_FILE ? 'F' : 'D', cur_path );
      
      free( cur_path );
   }

   printf("Iterator error: %d\n", fskit_path_iterator_error( itr ) );
   fskit_path_iterator_release( itr );
      
   
   for( itr = fskit_path_begin( core, "/././root///././/.//.", true ); !fskit_path_end( itr ); fskit_path_next( itr ) ) {
      
      struct fskit_entry* cur = fskit_path_iterator_entry( itr );
      char* cur_path = fskit_path_iterator_path( itr );
      
      printf("Entry %016" PRIX64 " (%p): %c %s\n", fskit_entry_get_file_id( cur ), cur, fskit_entry_get_type( cur ) == FSKIT_ENTRY_TYPE_FILE ? 'F' : 'D', cur_path );
      
      free( cur_path );
   }

   printf("Iterator error: %d\n", fskit_path_iterator_error( itr ) );
   fskit_path_iterator_release( itr );
      
   /////////////////////////////////////////////////////////////////////////////////////
   printf("Iterate fails (path too long)...\n");
   
   for( itr = fskit_path_begin( core, "/root/L/R/L/R/L/R/L/R/L/R/L/R/L/R/L/R", true ); !fskit_path_end( itr ); fskit_path_next( itr ) ) {
      
      struct fskit_entry* cur = fskit_path_iterator_entry( itr );
      char* cur_path = fskit_path_iterator_path( itr );
      
      printf("Entry %016" PRIX64 " (%p): %c %s\n", fskit_entry_get_file_id( cur ), cur, fskit_entry_get_type( cur ) == FSKIT_ENTRY_TYPE_FILE ? 'F' : 'D', cur_path );
      
      free( cur_path );
   }
   
   printf("Iterator error: %d\n", fskit_path_iterator_error( itr ) );
   fskit_path_iterator_release( itr );
   
   /////////////////////////////////////////////////////////////////////////////////////
   printf("Iterate fails (path does not exist)...\n");
   
   for( itr = fskit_path_begin( core, "/root/L/R/L/foo/L/R", true ); !fskit_path_end( itr ); fskit_path_next( itr ) ) {
      
      struct fskit_entry* cur = fskit_path_iterator_entry( itr );
      char* cur_path = fskit_path_iterator_path( itr );
      
      printf("Entry %016" PRIX64 " (%p): %c %s\n", fskit_entry_get_file_id( cur ), cur, fskit_entry_get_type( cur ) == FSKIT_ENTRY_TYPE_FILE ? 'F' : 'D', cur_path );
      
      free( cur_path );
   }
   
   printf("Iterator error: %d\n", fskit_path_iterator_error( itr ) );
   fskit_path_iterator_release( itr );
   
   fskit_test_end( core, &output );

   return 0;
}
