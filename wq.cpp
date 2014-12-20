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

#include "wq.h"
#include "util.h"
#include "common.h"

// work queue main method 
static void* fskit_wq_main( void* cls ) {
   
   struct fskit_wq* wq = (struct fskit_wq*)cls;
   fskit_wq_queue_t* work = NULL;
   struct fskit_wreq wreq;
   int rc = 0;
   
   while( wq->running ) {
      
      // wait for work 
      sem_wait( &wq->work_sem );
      
      // cancelled?
      if( !wq->running ) {
         break;
      }
      
      // exchange buffers--we have work 
      pthread_mutex_lock( &wq->work_lock );
      
      work = wq->work;
      
      if( wq->work == wq->work_1 ) {
         wq->work = wq->work_2;
      }
      else {
         wq->work = wq->work_1;
      }
      
      pthread_mutex_unlock( &wq->work_lock );
      
      // safe to use work buffer (we'll clear it out)
      while( work->size() > 0 ) {
         
         // next work
         wreq = work->front();
         work->pop();
         
         // carry out work 
         rc = (*wreq.work)( &wreq, wreq.work_data );
         
         // is this a promise?  if so, tell the caller that we've fulfilled it 
         if( wreq.flags & FSKIT_WQ_PROMISE ) {
            wreq.promise_ret = rc;
            sem_post( &wreq.promise_sem );
         }
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
   
   wq->work_1 = safe_new( fskit_wq_queue_t );
   wq->work_2 = safe_new( fskit_wq_queue_t );
   
   if( wq->work_1 == NULL || wq->work_2 == NULL ) {
      
      safe_delete( wq->work_1 );
      safe_delete( wq->work_2 );
      
      return -ENOMEM;
   }
   
   wq->work = wq->work_1;
   
   pthread_mutex_init( &wq->work_lock, NULL );
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
static int fskit_wq_queue_free( fskit_wq_queue_t* wqueue ) {
   
   if( wqueue != NULL ) {
      while( wqueue->size() > 0 ) {
         
         struct fskit_wreq wreq = wqueue->front();
         wqueue->pop();
         
         fskit_wreq_free( &wreq );
      }
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
   fskit_wq_queue_free( wq->work_1 );
   fskit_wq_queue_free( wq->work_2 );
   
   safe_delete( wq->work_1 );
   safe_delete( wq->work_2 );
   
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
   
   if( !wq->running ) {
      return -EINVAL;
   }
   
   pthread_mutex_lock( &wq->work_lock );
   
   try {
      wq->work->push( *wreq );
   }
   catch( bad_alloc& ba ) {
      rc = -ENOMEM;
   }
   
   pthread_mutex_unlock( &wq->work_lock );
   
   if( rc == 0 ) {
      // have work 
      sem_post( &wq->work_sem );
   }
   
   return rc;
}
