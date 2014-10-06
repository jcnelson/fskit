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

#ifndef _FSKIT_COMMON_H_
#define _FSKIT_COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <signal.h>
#include <libgen.h>
#include <regex.h>
#include <stdint.h>
#include <math.h>
#include <inttypes.h>
#include <stdarg.h>
#include <ctype.h>
#include <fcntl.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <semaphore.h>
#include <pthread.h>

#include <utime.h>

#include <iostream>
#include <list>
#include <map>
#include <vector>
#include <new>

#define MIN( x, y ) (x) > (y) ? (y) : (x)

using namespace std;

#endif