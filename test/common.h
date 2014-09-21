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

#ifndef _TEST_COMMON_H_
#define _TEST_COMMON_H_

#include "fskit.h"

void fskit_type_to_string( int type, char type_buf[10] );

int fskit_print_tree( FILE* out, struct fskit_entry* root );

int fskit_test_begin( struct fskit_core* core, void* test_data );
int fskit_test_end( struct fskit_core* core, void** test_data );

int fskit_test_mkdir_LR_recursive( struct fskit_core* core, char const* path, int depth );

#endif 