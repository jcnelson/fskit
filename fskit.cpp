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

#include "fskit.h"

// initialize the library 
int fskit_library_init() {
   int rc = 0;
   
   rc = fskit_random_init();
   if( rc != 0 ) {
      fskit_error("fskit_random_init rc = %d\n", rc );
      return rc;
   }
   
   return 0;
}

// shutdown the library 
int fskit_library_shutdown() {
   return 0;
}