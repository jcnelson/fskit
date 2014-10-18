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

#ifndef _FSKIT_TRUNC_H_
#define _FSKIT_TRUNC_H_

#include "debug.h"
#include "fskit.h"
#include "common.h"

int fskit_run_user_trunc( struct fskit_core* core, char const* path, struct fskit_entry* fent, off_t new_size );

int fskit_do_trunc( struct fskit_core* core, char const* path, struct fskit_entry* fent, off_t new_size );

extern "C" {

int fskit_trunc( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, off_t new_size );
int fskit_ftrunc( struct fskit_core* core, struct fskit_file_handle* fh, off_t new_size );

}

#endif