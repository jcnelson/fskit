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

#ifndef _FSKIT_STAT_H_
#define _FSKIT_STAT_H_

#include <fskit/common.h>
#include <fskit/entry.h>

#include <sys/stat.h>

FSKIT_C_LINKAGE_BEGIN

int fskit_entry_fstat( struct fskit_entry* fent, struct stat* sb );

int fskit_stat( struct fskit_core* core, char const* fs_path, uint64_t user, uint64_t group, struct stat* sb );
int fskit_fstat( struct fskit_core* core, char const* fs_path, struct fskit_entry* fent, struct stat* sb );

mode_t fskit_fullmode( int fskit_type, mode_t mode );

FSKIT_C_LINKAGE_END 

#endif
