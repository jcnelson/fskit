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

#ifndef _FSKIT_PATH_H_
#define _FSKIT_PATH_H_

#include "common.h"
#include "debug.h"
#include "entry.h"

// path utilities 
long fskit_entry_name_hash( char const* name );
void fskit_sanitize_path( char* path );

extern "C" {
   
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