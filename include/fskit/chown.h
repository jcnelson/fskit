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

#ifndef _FSKIT_CHOWN_H_
#define _FSKIT_CHOWN_H_

#include "debug.h"
#include "entry.h"

extern "C" {

int fskit_entry_set_group( struct fskit_entry* fent, uint64_t new_group );
int fskit_entry_set_owner( struct fskit_entry* fent, uint64_t new_user );
int fskit_entry_set_owner_and_group( struct fskit_entry* fent, uint64_t new_user, uint64_t new_group );

int fskit_chown( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, uint64_t new_user, uint64_t new_group );

}

#endif
