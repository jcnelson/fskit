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

#include "fuse-demo.h"

int main( int argc, char** argv ) {
   
   int rc = 0;
   struct fskit_fuse_state state;
   
   // set up 
   rc = fskit_fuse_init( &state, NULL );
   if( rc != 0 ) {
      fprintf(stderr, "fskit_fuse_init rc = %d\n", rc );
      exit(1);
   }
   
   // run 
   rc = fskit_fuse_main( &state, argc, argv );
   
   // shutdown 
   fskit_fuse_shutdown( &state, NULL );
   
   return rc;
}
