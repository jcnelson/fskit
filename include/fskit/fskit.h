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

#ifndef _FSKIT_H_
#define _FSKIT_H_

#include <fskit/common.h>
#include <fskit/debug.h>
#include <fskit/entry.h>
#include <fskit/path.h>
#include <fskit/random.h>

#include <fskit/access.h>
#include <fskit/chmod.h>
#include <fskit/chown.h>
#include <fskit/close.h>
#include <fskit/closedir.h>
#include <fskit/create.h>
#include <fskit/getxattr.h>
#include <fskit/link.h>
#include <fskit/listxattr.h>
#include <fskit/mkdir.h>
#include <fskit/mknod.h>
#include <fskit/open.h>
#include <fskit/opendir.h>
#include <fskit/path.h>
#include <fskit/read.h>
#include <fskit/readdir.h>
#include <fskit/readlink.h>
#include <fskit/removexattr.h>
#include <fskit/rename.h>
#include <fskit/route.h>
#include <fskit/rmdir.h>
#include <fskit/setxattr.h>
#include <fskit/stat.h>
#include <fskit/statvfs.h>
#include <fskit/symlink.h>
#include <fskit/sync.h>
#include <fskit/trunc.h>
#include <fskit/unlink.h>
#include <fskit/utime.h>
#include <fskit/write.h>

#define FSKIT_FILESYSTEM_TYPE 0x19880119

FSKIT_C_LINKAGE_BEGIN 

int fskit_library_init();
int fskit_library_shutdown();

FSKIT_C_LINKAGE_END 

#endif
