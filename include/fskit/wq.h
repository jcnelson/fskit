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

#ifndef _FSKIT_WQ_H_
#define _FSKIT_WQ_H_

#include <fskit/common.h>
#include <fskit/debug.h>

// work request flags
// treat a work request like a promise: let the caller block on it until it's fulfilled
#define FSKIT_WQ_PROMISE        0x1

// fskit workqueue request
struct fskit_wreq;

// fskit workqueue
struct fskit_wq;

// fskit workqueue callback type
typedef int (*fskit_wq_func_t)( struct fskit_wreq* wreq, void* cls );

int fskit_wq_init( struct fskit_wq* wq );
int fskit_wq_start( struct fskit_wq* wq );
int fskit_wq_stop( struct fskit_wq* wq );
int fskit_wq_free( struct fskit_wq* wq );

int fskit_wreq_init( struct fskit_wreq* wreq, fskit_wq_func_t work, void* work_data, int flags );
int fskit_wreq_free( struct fskit_wreq* wreq );

int fskit_wreq_promise_wait( struct fskit_wreq* wreq );
int fskit_wreq_promise_ret( struct fskit_wreq* wreq );

int fskit_wq_add( struct fskit_wq* wq, struct fskit_wreq* wreq );

#endif
