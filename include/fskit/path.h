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
int fskit_path_split( char* path, char** names );

// path resolution
struct fskit_entry* fskit_entry_resolve_path_cls( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, bool writelock, int* err, int (*ent_eval)( struct fskit_entry*, void* ), void* cls );
struct fskit_entry* fskit_entry_resolve_path( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, bool writelock, int* err );

// path iteration struct 
struct fskit_path_iterator {
   
   struct fskit_core* core;
   
   char const* path;
   
   bool writelock;
   
   struct fskit_entry* prev_ent;
   struct fskit_entry* cur_ent;
   
   char* name;
   size_t name_i;
   
   bool end_of_path;
   
   int rc;
};

// path iteration
struct fskit_path_iterator fskit_path_begin( struct fskit_core* core, char const* path, bool writelock );
bool fskit_path_end( struct fskit_path_iterator* itr );
void fskit_path_next( struct fskit_path_iterator* itr );

// path iterator getters
int fskit_path_iterator_error( struct fskit_path_iterator* itr );
struct fskit_entry* fskit_path_iterator_entry( struct fskit_path_iterator* itr );
struct fskit_entry* fskit_path_iterator_entry_parent( struct fskit_path_iterator* itr );
void fskit_path_iterator_release( struct fskit_path_iterator* itr );
char* fskit_path_iterator_path( struct fskit_path_iterator* itr );

// referencing 
struct fskit_entry* fskit_entry_ref( struct fskit_core* core, char const* fs_path, int* rc );
int fskit_entry_ref_entry( struct fskit_entry* fent );
int fskit_entry_unref( struct fskit_core* core, char const* fs_path, struct fskit_entry* fent );

}

#endif
