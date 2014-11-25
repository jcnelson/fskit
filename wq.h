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

#ifndef _FSKIT_WQ_H_
#define _FSKIT_WQ_H_

#include "common.h"
#include "debug.h"

#include <queue>

// work request flags 
// treat a work request like a promise: let the caller block on it until it's fulfilled
#define FSKIT_WQ_PROMISE        0x1

using namespace std;

// fskit workqueue callback type
typedef int (*fskit_wq_func_t)( struct fskit_wreq* wreq, void* cls );

// fskit workqueue request
struct fskit_wreq {
   
   // callback to do work 
   fskit_wq_func_t work;
   
   // user-supplied arguments
   void* work_data;
   
   // flags controlling the lifecycle of this work request
   int flags;
   
   // promise semaphore, to wake up the caller.
   // only initialized of FSKIT_WQ_PROMISE is specified
   sem_t promise_sem;
   int promise_ret;
};

// workqueue type 
typedef queue< struct fskit_wreq > fskit_wq_queue_t;

// fskit workqueue
struct fskit_wq {
  
   // worker thread 
   pthread_t thread;
   
   // is the thread running?
   bool running;
   
   // things to do (double-bufferred)
   fskit_wq_queue_t* work;
   
   fskit_wq_queue_t* work_1;
   fskit_wq_queue_t* work_2;
   
   // lock governing access to work
   pthread_mutex_t work_lock;
   
   // semaphore to signal the availability of work
   sem_t work_sem;
};

extern "C" {
 
int fskit_wq_init( struct fskit_wq* wq );
int fskit_wq_start( struct fskit_wq* wq );
int fskit_wq_stop( struct fskit_wq* wq );
int fskit_wq_free( struct fskit_wq* wq );

int fskit_wreq_init( struct fskit_wreq* wreq, fskit_wq_func_t work, void* work_data, int flags );
int fskit_wreq_free( struct fskit_wreq* wreq );

int fskit_wreq_promise_wait( struct fskit_wreq* wreq );
int fskit_wreq_promise_ret( struct fskit_wreq* wreq );

int fskit_wq_add( struct fskit_wq* wq, struct fskit_wreq* wreq );

}

#endif 