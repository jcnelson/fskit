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

#ifndef _FSKIT_CREATE_H_
#define _FSKIT_CREATE_H_

#include "common.h"
#include "entry.h"

int fskit_run_user_create( struct fskit_core* core, char const* path, struct fskit_entry* fent, int flags, mode_t mode, void** inode_data, void** handle_data );
int fskit_do_create( struct fskit_core* core, struct fskit_entry* parent, char const* path, int flags, mode_t mode, uint64_t user, uint64_t group, struct fskit_entry** ret_child, void** handle_data );

extern "C" {
   
struct fskit_file_handle* fskit_create( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, mode_t mode, int* err );

}

#endif