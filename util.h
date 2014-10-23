/*
   fskit: a library for creating multi-threaded in-RAM filesystems
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

// do not include this in any fskit headers

#ifndef _FSKIT_UTIL_H_
#define _FSKIT_UTIL_H_

#include "common.h"

#define CALLOC_LIST(type, count) (type*)calloc( sizeof(type) * (count), 1 )
#define FREE_LIST(list) do { if( (list) != NULL ) { for(unsigned int __i = 0; (list)[__i] != NULL; ++ __i) { if( (list)[__i] != NULL ) { free( (list)[__i] ); (list)[__i] = NULL; }} free( (list) ); } } while(0)

#define strdup_or_null( str )  (str) != NULL ? strdup(str) : NULL
#define safe_free( ptr ) do { if( (ptr) != NULL ) { free( ptr ); (ptr) = NULL; } } while(0)
#define safe_delete( ptr ) do { if( (ptr) != NULL ) { delete (ptr); (ptr) = NULL; } } while(0)
#define safe_new( cls ) new (nothrow) cls()

#endif
