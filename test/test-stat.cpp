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

#include "test-stat.h"

int main( int argc, char** argv ) {
   
   struct fskit_core core;
   int rc;
   char name_buf[10];
   struct fskit_file_handle* fh = NULL;
   void* output;
   
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
      
      struct stat sb;
      memset( &sb, 0xFF, sizeof(struct stat) );

      rc = fskit_stat( &core, name_buf, 0, i, &sb );
      
      if( rc != 0 ) {
         errorf("fskit_stat('%s') rc = %d\n", name_buf, rc );
         exit(1);
      }
      
      dbprintf("%s: stat(st_dev=%lX st_ino=%lX st_mode=%o st_nlink=%lu st_uid=%d st_gid=%d st_rdev=%lX st_size=%jd st_blksize=%ld st_blocks=%ld)\n",
               name_buf, sb.st_dev, sb.st_ino, sb.st_mode, sb.st_nlink, sb.st_uid, sb.st_gid, sb.st_rdev, sb.st_size, sb.st_blksize, sb.st_blocks );
   }
   
   fskit_print_tree( stdout, &core.root );
   
   fskit_test_end( &core, &output );
   
   return 0;
}