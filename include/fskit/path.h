/*
   fskit: a library for creating multi-threaded in-RAM filesystems
   Copyright (C) 2014  Jude Nelson

   This program is dual-licensed: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License version 3 or later as
   published by the Free Software Foundation. For the terms of this
   license, see LICENSE.LGPLv3+ or <http://www.gnu.org/licenses/>.

   You are free to use this program under the terms of the GNU Lesser General
   Public License, but WITHOUT ANY WARRANTY; without even the implied
   warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   See the GNU Lesser General Public License for more details.

   Alternatively, you are free to use this program under the terms of the
   Internet Software Consortium License, but WITHOUT ANY WARRANTY; without
   even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   For the terms of this license, see LICENSE.ISC or
   <http://www.isc.org/downloads/software-support-policy/isc-license/>.
*/

#ifndef _FSKIT_PATH_H_
#define _FSKIT_PATH_H_

#include "common.h"
#include "debug.h"
#include "entry.h"


extern "C" {

// path utilities
long fskit_entry_name_hash( char const* name );
void fskit_sanitize_path( char* path );

// public path utilities
char* fskit_fullpath( char const* parent, char const* child, char* output );
char* fskit_dirname( char const* path, char* dest );
char* fskit_basename( char const* path, char* dest );
size_t fskit_basename_len( char const* path );
int fskit_depth( char const* path );

// path resolution
struct fskit_entry* fskit_entry_resolve_path_cls( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, bool writelock, int* err, int (*ent_eval)( struct fskit_entry*, void* ), void* cls );
struct fskit_entry* fskit_entry_resolve_path( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, bool writelock, int* err );

}

#endif
