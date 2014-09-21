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

#ifndef _FSKIT_UTIME_H_
#define _FSKIT_UTIME_H_

#include "common.h"
#include "entry.h"

#ifndef _UTIME_H
struct utimbuf {
   time_t actime;
   time_t modtime;
};
#endif

extern "C" {
   
// time 
int fskit_entry_set_ctime( struct fskit_entry* fent, struct timespec* now );
int fskit_entry_set_mtime( struct fskit_entry* fent, struct timespec* now );
int fskit_entry_set_atime( struct fskit_entry* fent, struct timespec* now );

// POSIX methods 
int fskit_utime( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, const struct utimbuf* times );
int fskit_utimes( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, const struct timeval times[2] );

}

#endif