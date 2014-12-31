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

#include "common.h"
#include "debug.h"
#include "deferred.h"
#include "entry.h"
#include "path.h"
#include "random.h"
#include "wq.h"

#include "access.h"
#include "chmod.h"
#include "chown.h"
#include "close.h"
#include "closedir.h"
#include "create.h"
#include "getxattr.h"
#include "link.h"
#include "listxattr.h"
#include "mkdir.h"
#include "mknod.h"
#include "open.h"
#include "opendir.h"
#include "path.h"
#include "read.h"
#include "readdir.h"
#include "readlink.h"
#include "removexattr.h"
#include "rename.h"
#include "route.h"
#include "rmdir.h"
#include "setxattr.h"
#include "stat.h"
#include "statvfs.h"
#include "symlink.h"
#include "sync.h"
#include "trunc.h"
#include "unlink.h"
#include "utime.h"
#include "write.h"

#define FSKIT_FILESYSTEM_TYPE 0x19880119
#define FSKIT_FILESYSTEM_NAMEMAX 255

extern "C" {
   
int fskit_library_init();
int fskit_library_shutdown();

}

#endif 