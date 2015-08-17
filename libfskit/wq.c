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

#include <fskit/wq.h>
#include <fskit/util.h>
#include <fskit/common.h>

#include "fskit_private/private.h"

// work queue main method
static void* fskit_wq_main( void* cls ) {
   
   struct fskit_wq* wq = (struct fskit_wq*)cls;
   
   struct fskit_wreq* work_itr = NULL;
   struct fskit_wreq* next = NULL;
   
   int rc = 0;

   while( wq->running ) {

      // is there work?
      rc = sem_trywait( &wq->work_sem );
      if( rc != 0 ) {
         
         rc = -errno;
         if( rc == -EAGAIN ) {
            
            // wait for work
            sem_wait( &wq->work_sem );
         }
         else {
            
            // some other fatal error 
            fskit_error("FATAL: sem_trywait rc = %d\n", rc );
            break;
         }
      }
      
      // cancelled?
      if( !wq->running ) {
         break;
      }

      // exchange buffers--we have work to do
      pthread_mutex_lock( &wq->work_lock );

      work_itr = wq->work;
      wq->work = NULL;
      wq->tail = NULL;
      
      pthread_mutex_unlock( &wq->work_lock );

      // safe to use work buffer (we'll clear it out in the process)
      while( work_itr != NULL ) {

         // carry out work
         rc = (*work_itr->work)( work_itr, work_itr->work_data );
         
         if( rc != 0 ) {
            
            fskit_error("work %p rc = %d\n", work_itr->work, rc );
         }
         
         
         next = work_itr->next;
         
         if( work_itr->flags & FSKIT_WQ_PROMISE ) {
            
            // this is a promise--the caller will wait on it and free it
            work_itr->promise_ret = rc;
         }
         else {
            
            fskit_wreq_free( work_itr );
            fskit_safe_free( work_itr );
         }
         
         work_itr = next;
      }
   }

   return NULL;
}


// set up a work queue, but don't start it.
// return 0 on success
// return negative on failure:
// * -ENOMEM if OOM
int fskit_wq_init( struct fskit_wq* wq ) {

   
   int rc = 0;

   memset( wq, 0, sizeof(struct fskit_wq) );
   
   rc = pthread_mutex_init( &wq->work_lock, NULL );
   if( rc != 0 ) {
      
      return -abs(rc);
   }
   
   sem_init( &wq->work_sem, 0, 0 );
   
   return rc;
}


// start a work queue
// return 0 on success
// return negative on error:
// * -EINVAL if already started
// * whatever pthread_create errors on
int fskit_wq_start( struct fskit_wq* wq ) {

   
   if( wq->running ) {
      return -EINVAL;
   }

   int rc = 0;
   pthread_attr_t attrs;

   memset( &attrs, 0, sizeof(pthread_attr_t) );

   wq->running = true;

   rc = pthread_create( &wq->thread, &attrs, fskit_wq_main, wq );
   if( rc != 0 ) {

      wq->running = false;

      rc = -errno;
      fskit_error("pthread_create errno = %d\n", rc );

      return rc;
   }

   return 0;
}

// stop a work queue
// return 0 on success
// return negative on error:
// * -EINVAL if not running
int fskit_wq_stop( struct fskit_wq* wq ) {

   if( !wq->running ) {
      return -EINVAL;
   }

   wq->running = false;

   // wake up the work queue so it cancels
   sem_post( &wq->work_sem );
   pthread_cancel( wq->thread );

   pthread_join( wq->thread, NULL );

   return 0;
}


// free a work request queue
static int fskit_wq_queue_free( struct fskit_wreq* wqueue ) {

   struct fskit_wreq* next = NULL;
   
   while( wqueue != NULL ) {
      
      next = wqueue->next;
      
      fskit_wreq_free( wqueue );
      fskit_safe_free( wqueue );
      
      wqueue = next;
   }
   
   return 0;
}

// free up a work queue
// return 0 on success
// return negative on error:
// * -EINVAL if running
int fskit_wq_free( struct fskit_wq* wq ) {

   if( wq->running ) {
      return -EINVAL;
   }

   // free all
   fskit_wq_queue_free( wq->work );

   pthread_mutex_destroy( &wq->work_lock );
   sem_destroy( &wq->work_sem );

   memset( wq, 0, sizeof(struct fskit_wq) );

   return 0;
}

// create a work request
int fskit_wreq_init( struct fskit_wreq* wreq, fskit_wq_func_t work, void* work_data, int flags ) {

   memset( wreq, 0, sizeof(struct fskit_wreq) );

   wreq->work = work;
   wreq->work_data = work_data;
   wreq->flags = flags;

   if( flags & FSKIT_WQ_PROMISE ) {
      // set up promise fields
      sem_init( &wreq->promise_sem, 0, 0 );
      wreq->promise_ret = 0;
   }

   return 0;
}

// free a work request
int fskit_wreq_free( struct fskit_wreq* wreq ) {

   if( wreq->flags & FSKIT_WQ_PROMISE ) {

      sem_destroy( &wreq->promise_sem );
   }

   memset( wreq, 0, sizeof(struct fskit_wreq) );
   return 0;
}

// wait for a work request (promise) to complete
// return 0 on success
// return negative on error;
// * -EINVAL if the work request was not initialized with FSKIT_WQ_PROMISE
int fskit_wreq_promise_wait( struct fskit_wreq* wreq ) {

   if( (wreq->flags & FSKIT_WQ_PROMISE) == 0 ) {
      return -EINVAL;
   }

   sem_wait( &wreq->promise_sem );

   return 0;
}

// get the result of a work request (promise)
// return -EINVAL if this isn't a promise
int fskit_wreq_promise_ret( struct fskit_wreq* wreq ) {

   if( (wreq->flags & FSKIT_WQ_PROMISE) == 0 ) {
      return -EINVAL;
   }

   return wreq->promise_ret;
}

// enqueue work
// return 0 on success
// return -EINVAL if the work queue thread isn't running
int fskit_wq_add( struct fskit_wq* wq, struct fskit_wreq* wreq ) {

   
   int rc = 0;
   struct fskit_wreq* next = NULL;

   // duplicate this work item 
   next = CALLOC_LIST( struct fskit_wreq, 1 );
   if( next == NULL ) {
      return -ENOMEM;
   }
   
   next->work = wreq->work;
   next->work_data = wreq->work_data;
   next->next = NULL;
   
   pthread_mutex_lock( &wq->work_lock );
   
   if( wq->work == NULL ) {
      // head
      wq->work = next;
      wq->tail = next;
   }
   else {
      // append 
      wq->tail->next = next;
      wq->tail = next;
   }
   
   pthread_mutex_unlock( &wq->work_lock );

   if( rc == 0 ) {
      // have work
      sem_post( &wq->work_sem );
   }
   
   return rc;
}
