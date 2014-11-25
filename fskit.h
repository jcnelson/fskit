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
#include "listxattr.h"
#include "mkdir.h"
#include "mknod.h"
#include "open.h"
#include "opendir.h"
#include "path.h"
#include "read.h"
#include "readdir.h"
#include "removexattr.h"
#include "rename.h"
#include "route.h"
#include "rmdir.h"
#include "setxattr.h"
#include "stat.h"
#include "statvfs.h"
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