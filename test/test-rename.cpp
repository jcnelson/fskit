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

#include "test-rename.h"

int main( int argc, char** argv ) {
   
   struct fskit_core core;
   int rc;
   char name_buf[10];
   char name_buf2[10];
   
   struct fskit_file_handle* fh = NULL;
   void* output;
   
   rc = fskit_test_begin( &core, NULL );
   if( rc != 0 ) {
      exit(1);
   }
   
   // setup
   for( int i = 0; i < 10; i++ ) {
      
      // create /a$i
      
      memset(name_buf, 0, 10 );
      sprintf(name_buf, "/a%d", i );
      
      fh = fskit_create( &core, name_buf, 0, i, 0644, &rc );
      
      if( fh == NULL ) {
         errorf("fskit_create('%s') rc = %d\n", name_buf, rc );
         exit(1);
      }
      
      fskit_close( &core, fh );
      
      // mkdir /d$i
      
      memset(name_buf, 0, 10 );
      sprintf(name_buf, "/d%d", i );
      
      rc = fskit_mkdir( &core, name_buf, 0755, 0, 0 );
      if( rc != 0 ) {
         errorf("fskit_mkdir('%s') rc = %d\n", name_buf, rc );
         exit(1);
      }
   }
   
   printf("Initial tree:\n");
   fskit_print_tree( stdout, &core.root );
   
   // rename in the same directory
   // rename /a$i to /b$i
   for( int i = 0; i < 10; i++ ) {
      
      memset(name_buf, 0, 10 );
      memset(name_buf2, 0, 10 );
      
      sprintf(name_buf, "/a%d", i );
      sprintf(name_buf2, "/b%d", i );
      
      rc = fskit_rename( &core, name_buf, name_buf2, 0, 0 );
      if( rc != 0 ) {
         errorf("fskit_rename('%s', '%s') rc = %d\n", name_buf, name_buf2, rc );
         exit(1);
      }
   }
   
   printf("Rename /a$i to /b$i");
   fskit_print_tree( stdout, &core.root );
   
   // rename into a deeper directory 
   // rename /b$i to /d$i/a$i
   for( int i = 0; i < 10; i++ ) {
      
      memset(name_buf, 0, 10 );
      memset(name_buf2, 0, 10 );
      
      sprintf(name_buf, "/b%d", i );
      sprintf(name_buf2, "/d%d/a%d", i, i );
      
      rc = fskit_rename( &core, name_buf, name_buf2, 0, 0 );
      if( rc != 0 ) {
         errorf("fskit_rename('%s', '%s') rc = %d\n", name_buf, name_buf2, rc );
         exit(1);
      }
   }
   
   
   printf("Rename /b$i to /d$i/a$i");
   fskit_print_tree( stdout, &core.root );
   
   // rename into a shallower directory 
   // rename /d$i/a$i to /a$i
   for( int i = 0; i < 10; i++ ) {
      
      memset(name_buf, 0, 10 );
      memset(name_buf2, 0, 10 );
      
      sprintf(name_buf, "/d%d/a%d", i, i );
      sprintf(name_buf2, "/a%d", i );
      
      rc = fskit_rename( &core, name_buf, name_buf2, 0, 0 );
      if( rc != 0 ) {
         errorf("fskit_rename('%s', '%s') rc = %d\n", name_buf, name_buf2, rc );
         exit(1);
      }
   }
   
   printf("Rename /d/a$i to /a$i");
   fskit_print_tree( stdout, &core.root );
   
   fskit_test_end( &core, &output );
   
   return 0;
}