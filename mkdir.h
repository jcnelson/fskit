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

#ifndef _FSKIT_MKDIR_H_
#define _FSKIT_MKDIR_H_

#include "entry.h"

int fskit_run_user_mkdir( struct fskit_core* core, char const* path, struct fskit_entry* fent, mode_t mode, void** inode_data );

extern "C" {
   
int fskit_mkdir( struct fskit_core* core, char const* path, mode_t mode, uint64_t user, uint64_t group );

}

#endif