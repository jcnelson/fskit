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

#ifndef _FSKIT_READDIR_H_
#define _FSKIT_READDIR_H_

#include <fskit/debug.h>
#include <fskit/entry.h>

FSKIT_C_LINKAGE_BEGIN 

struct fskit_dir_entry** fskit_readdir( struct fskit_core* core, struct fskit_dir_handle* dirh, uint64_t num_children, uint64_t* num_read, int* err );
struct fskit_dir_entry** fskit_listdir( struct fskit_core* core, struct fskit_dir_handle* dirh, uint64_t* num_read, int* err );

void fskit_dir_entry_free_list( struct fskit_dir_entry** dir_ents );
void fskit_dir_entry_free( struct fskit_dir_entry* d_ent );

int fskit_readdir_omit( struct fskit_dir_entry** dents, int i );

void fskit_seekdir( struct fskit_dir_handle* dirh, off_t loc );
off_t fskit_telldir( struct fskit_dir_handle* dirh );
void fskit_rewinddir( struct fskit_dir_handle* dirh );

FSKIT_C_LINKAGE_END 

#endif
