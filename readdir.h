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

#ifndef _FSKIT_READDIR_H_
#define _FSKIT_READDIR_H_

#include "entry.h"


extern "C" {
   
struct fskit_dir_entry** fskit_readdir( struct fskit_core* core, struct fskit_dir_handle* dirh, uint64_t child_offset, uint64_t num_children, uint64_t* num_read, int* err );
struct fskit_dir_entry** fskit_listdir( struct fskit_core* core, struct fskit_dir_handle* dirh, uint64_t* num_read, int* err );

void fskit_dir_entry_free_list( struct fskit_dir_entry** dir_ents );
void fskit_dir_entry_free( struct fskit_dir_entry* d_ent );

}

#endif