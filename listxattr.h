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

#ifndef _FSKIT_LISTXATTR_H_
#define _FSKIT_LISTXATTR_H_

#include "debug.h"
#include "entry.h"

extern "C" {

int fskit_listxattr( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, char* list, size_t size );
int fskit_flistxattr( struct fskit_core* core, struct fskit_entry* fent, char* list, size_t size );

}

#endif