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

#include "test-utime.h"

int main( int argc, char** argv ) {
   
   
   struct fskit_core core;
   int rc;
   char name_buf[10];
   struct fskit_file_handle* fh = NULL;
   void* output;
   
   
   const struct timeval times[2] = {
      {12345, 67890},
      {23456, 78901}
   };
   
   rc = fskit_test_begin( &core, NULL );
   if( rc != 0 ) {
      exit(1);
   }
   
   for( int i = 0; i < 256; i++ ) {
      
      sprintf(name_buf, "/%d", i );
      
      fh = fskit_create( &core, name_buf, 0, i, 0644, &rc );
      
      if( fh == NULL ) {
         errorf("fskit_create('%s') rc = %d\n", name_buf, rc );
         exit(1);
      }
      
      fskit_close( &core, fh );
   }
   
   for( int i = 0; i < 256; i++ ) {
      
      sprintf(name_buf, "/%d", i );
      
      rc = fskit_utimes( &core, name_buf, 0, i, times );
      
      if( rc != 0 ) {
         errorf("fskit_utimes('%s') rc = %d\n", name_buf, rc );
         exit(1);
      }
   }
   
   fskit_print_tree( stdout, &core.root );
   
   fskit_test_end( &core, &output );
   
   return 0;
}