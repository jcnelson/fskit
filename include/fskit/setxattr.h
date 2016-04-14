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

#ifndef _FSKIT_SETXATTR_H_
#define _FSKIT_SETXATTR_H_

#include <fskit/debug.h>
#include <fskit/entry.h>

#include <attr/xattr.h>

FSKIT_C_LINKAGE_BEGIN 

int fskit_run_user_setxattr( struct fskit_core* core, char const* path, struct fskit_entry* fent, char const* xattr_name, char const* xattr_value, size_t xattr_value_len );
int fskit_fsetxattr( struct fskit_core* core, char const* path, struct fskit_entry* fent, char const* name, char const* value, size_t value_len, int flags );
int fskit_setxattr( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, char const* name, char const* value, size_t value_len, int flags );

FSKIT_C_LINKAGE_END 

#endif
