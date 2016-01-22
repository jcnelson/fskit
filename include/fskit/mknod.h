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


#ifndef _FSKIT_MKNOD_H_
#define _FSKIT_MKNOD_H_

#include <fskit/debug.h>
#include <fskit/common.h>
#include <fskit/entry.h>

FSKIT_C_LINKAGE_BEGIN 

int fskit_mknod( struct fskit_core* core, char const* path, mode_t mode, dev_t dev, uint64_t user, uint64_t group );
int fskit_mknod_ex( struct fskit_core* core, char const* path, mode_t mode, dev_t dev, uint64_t user, uint64_t group, void* cls );

FSKIT_C_LINKAGE_END 

#endif
