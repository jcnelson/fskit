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

#include "entry.h"
#include "route.h"
#include "util.h"

// initialize a match group.
// the match group becomes the owner of matches
// return 0 on success
static int fskit_match_group_init( struct fskit_match_group* match_group, char* matched_path, int num_matches, char** matches ) {
   memset( match_group, 0, sizeof(struct fskit_match_group) );
   
   // shallow copy 
   match_group->argc = num_matches;
   match_group->argv = matches;
   match_group->path = matched_path;
   
   return 0;
}


// free a match group 
// return 0 on success
static int fskit_match_group_free( struct fskit_match_group* match_group ) {
   
   if( match_group->argv != NULL ) {
      
      for( int i = 0; i < match_group->argc; i++ ) {
         
         if( match_group->argv[i] != NULL ) {

            free( match_group->argv[i] );
            match_group->argv[i] = NULL;
         }  
      }
      
      free( match_group->argv );
      match_group->argv = NULL;
   }
   
   
   if( match_group->path != NULL ) {
      
      safe_free( match_group->path );
      match_group->path = NULL;
   }
   
   memset( match_group, 0, sizeof(struct fskit_match_group) );
   
   return 0;
}


// how many expected matches in a regex?
// return the number, which is a lower bound
static int fskit_num_expected_matches( char const* path ) {
   
   size_t path_len = strlen(path);
   char prev = 0;
   int num_groups = 0;
   
   for( unsigned int i = 0; i < path_len; i++ ) {
      
      // unescaped open-paren?
      if( path[i] == '(' && prev != '\\' ) {
         num_groups++;
      }
      
      prev = path[i];
   }
   
   return num_groups + 1;
}

// match a path against a regex, and fill in the given match group with the matched strings
// return 0 on success, -ENOMEM on oom
static int fskit_match_regex( struct fskit_match_group* match_group, struct fskit_path_route* route, char const* path ) {
   
   int rc = 0;
   regmatch_t* m = CALLOC_LIST( regmatch_t, route->num_expected_matches + 1 );
   size_t path_len = strlen(path);

   if( m == NULL ) {
      
      return -ENOMEM;
   }
   
   rc = regexec( &route->path_regex, path, route->num_expected_matches, m, 0 );

   if( rc != 0 ) {
      // no matches 
      safe_free( m );
      return -ENOENT;
   }
   
   // sanity check 
   if( m[0].rm_so < 0 || m[0].rm_eo < 0 ) {
      // no match 
      safe_free( m );
      return -ENOENT;
   }
   
   // matched! whole path?
   if( (signed)path_len != m[0].rm_eo - m[0].rm_so ) {
      // didn't match the whole path 
      fskit_debug("Matched only %d:%d of 0:%zu\n", m[0].rm_so, m[0].rm_eo, path_len );
      safe_free( m );
      return -ENOENT;
   }
   
   char** argv = CALLOC_LIST( char*, route->num_expected_matches + 1 );
   if( argv == NULL ) {
      
      safe_free( m );
      return -ENOMEM;
   }
   
   char* path_dup = strdup( path );
   if( path_dup == NULL ) {
      
      FREE_LIST( argv );
      safe_free( m );
      return -ENOMEM;
   }
   
   // accumulate matches 
   int i = 1;
   for( i = 1; i <= route->num_expected_matches && m[i].rm_so >= 0 && m[i].rm_eo >= 0; i++ ) {
      
      char* next_match = CALLOC_LIST( char, m[i].rm_eo - m[i].rm_so + 1 );
      if( next_match == NULL ) {
         
         FREE_LIST( argv );
         safe_free( argv );
         safe_free( m );
         return -ENOMEM;
      }
      
      strncpy( next_match, path + m[i].rm_so, m[i].rm_eo - m[i].rm_so );
      
      argv[i-1] = next_match;
   }
   
   // i is the number of args 
   fskit_match_group_init( match_group, path_dup, i, argv );
   
   safe_free( m );
   return 0;
}


// is a route defined?
static bool fskit_path_route_is_defined( struct fskit_path_route* route ) {
   
   // a route is defined if it has a defined path regex
   return route->path_regex_str != NULL;
}

// start running a route's callback.
// enforce the consistency discipline by locking the route appropriately
static int fskit_route_enter( struct fskit_path_route* route, struct fskit_entry* fent ) {
   
   int rc = 0;
   
   // enforce the consistency discipline for this route
   if( route->consistency_discipline == FSKIT_SEQUENTIAL ) {
      rc = pthread_rwlock_wrlock( &route->lock );
   }
   else if( route->consistency_discipline == FSKIT_CONCURRENT ) {
      rc = pthread_rwlock_rdlock( &route->lock );
   }
   else if( route->consistency_discipline == FSKIT_INODE_SEQUENTIAL ) {
      rc = fskit_entry_wlock( fent );
   }

   if( rc != 0 ) {
      // indicates deadlock 
      rc = -errno;
      fskit_error("BUG: locking route %s errno = %d\n", route->path_regex_str, rc );
      return rc;
   }
   
   return 0;
}

// finish running a route's callback.
// clean up from enforcing the consistency discipline 
static int fskit_route_leave( struct fskit_path_route* route, struct fskit_entry* fent ) {
   
   if( route->consistency_discipline == FSKIT_INODE_SEQUENTIAL ) {
      fskit_entry_unlock( fent );
   }
   
   pthread_rwlock_unlock( &route->lock );
   return 0;
}

#define fskit_safe_dispatch( method, ... ) ((method) == NULL ? -ENOSYS : (*method)( __VA_ARGS__ ))

// dispatch a route
// return the result of the callback, or -ENOSYS if the callback is NULL
static int fskit_route_dispatch( struct fskit_core* core, struct fskit_match_group* match_group, struct fskit_path_route* route, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs ) {
   
   int rc = 0;
   
   // enforce the consistency discipline 
   rc = fskit_route_enter( route, fent );
   if( rc != 0 ) {
      return rc;
   }

   if( rc != 0 ) {
      // indicates deadlock 
      rc = -errno;
      fskit_error("BUG: pthread_rwlock_wrlock(route %s) errno = %d\n", route->path_regex_str, rc );
      return rc;
   }
   
   
   switch( route->route_type ) {
      
      case FSKIT_ROUTE_MATCH_CREATE:
         
         rc = fskit_safe_dispatch( route->method.create_cb, core, match_group, fent, dargs->mode, &dargs->inode_data, &dargs->handle_data );
         break;
      
      case FSKIT_ROUTE_MATCH_MKNOD:
         
         rc = fskit_safe_dispatch( route->method.mknod_cb, core, match_group, fent, dargs->mode, dargs->dev, &dargs->inode_data );
         break;
         
      case FSKIT_ROUTE_MATCH_MKDIR:
         
         rc = fskit_safe_dispatch( route->method.mkdir_cb, core, match_group, fent, dargs->mode, &dargs->inode_data );
         break;
      
      case FSKIT_ROUTE_MATCH_OPEN:
         
         rc = fskit_safe_dispatch( route->method.open_cb, core, match_group, fent, dargs->flags, &dargs->handle_data );
         break; 
      
      case FSKIT_ROUTE_MATCH_READDIR:
         
         rc = fskit_safe_dispatch( route->method.readdir_cb, core, match_group, fent, dargs->dents, dargs->num_dents );
         break;
         
      case FSKIT_ROUTE_MATCH_READ:
      case FSKIT_ROUTE_MATCH_WRITE:
         
         rc = fskit_safe_dispatch( route->method.io_cb, core, match_group, fent, dargs->iobuf, dargs->iolen, dargs->iooff, dargs->handle_data );
         break;
         
      case FSKIT_ROUTE_MATCH_TRUNC:
         
         rc = fskit_safe_dispatch( route->method.trunc_cb, core, match_group, fent, dargs->iooff, dargs->handle_data );
         break;
         
      case FSKIT_ROUTE_MATCH_CLOSE:
      
         rc = fskit_safe_dispatch( route->method.close_cb, core, match_group, fent, dargs->handle_data );
         break;
      
      case FSKIT_ROUTE_MATCH_DETACH:
         
         rc = fskit_safe_dispatch( route->method.detach_cb, core, match_group, fent, dargs->inode_data );
         break;
         
      case FSKIT_ROUTE_MATCH_STAT:
         
         rc = fskit_safe_dispatch( route->method.stat_cb, core, match_group, fent, dargs->sb );
         break;
         
      case FSKIT_ROUTE_MATCH_SYNC:
         
         rc = fskit_safe_dispatch( route->method.sync_cb, core, match_group, fent );
         break;
         
      default:
         
         fskit_error("Invalid route dispatch code %d\n", route->route_type );
         rc = -EINVAL;
   }
   
   fskit_route_leave( route, fent );
   
   return rc;
}


// try to match a path and type to a route.
// we consider it "found" if we can match on a regex in the route table.
// return the index into the route table of the first matching route, and populate match_group with the matched data.
// return -EPERM if not found
static int fskit_route_match( fskit_route_table_t* route_table, int route_type, char const* path, struct fskit_match_group* match_group ) {
   
   // find the route list
   fskit_route_table_t::iterator itr = route_table->find( route_type );
   if( itr == route_table->end() ) {
      
      // no such operations defined 
      return -EPERM;
   }
   
   int rc = 0;
   fskit_route_list_t* routes = &itr->second;
   
   for( unsigned int i = 0; i < routes->size(); i++ ) {
      
      if( !fskit_path_route_is_defined( &routes->at(i) ) ) {
         continue;
      }
      
      // match?
      rc = fskit_match_regex( match_group, &routes->at(i), path );
      if( rc == 0 ) {
         
         // matched!
         return i;
      }
   }
   
   // no match
   return -EPERM;
}


// call a route 
// return 0 on success, -EPERM if no route found
// place the callback status in *cbrc, if called.
int fskit_route_call( struct fskit_core* core, int route_type, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
   
   int route_index = 0;
   struct fskit_match_group match_group;
   fskit_route_list_t* routes = NULL;
   struct fskit_path_route* route = NULL;
   
   memset( &match_group, 0, sizeof(struct fskit_match_group) );
   
   // stop routes from getting changed out from under us
   fskit_core_route_rlock( core );
   
   route_index = fskit_route_match( core->routes, route_type, path, &match_group );
   
   if( route_index < 0 ) {
      // no route found 
      fskit_core_route_unlock( core );
      return -EPERM;
   }
   
   // found.  look it up 
   fskit_route_table_t::iterator itr = core->routes->find( route_type );
   if( itr == core->routes->end() ) {
      
      // should not happen if fskit_route_match succeeded, but you never know...
      fskit_core_route_unlock( core );
      fskit_match_group_free( &match_group );
      return -EPERM;
   }
   
   routes = &itr->second;
   
   if( route_index >= (signed)routes->size() ) {
      
      // should not happen since route_index is an index into routes, but you never know...
      fskit_core_route_unlock( core );
      fskit_match_group_free( &match_group );
      return -EPERM;
   }
   
   route = &routes->at(route_index);
   
   // dispatch 
   *cbrc = fskit_route_dispatch( core, &match_group, route, fent, dargs );
   
   fskit_core_route_unlock( core );
   
   fskit_match_group_free( &match_group );
   
   return 0;
}


// call the route to create a file.  The requisite inode_data and handle_data will be set in dargs on success.
// return 0 if a route was called, or -EPERM if there are no routes.
// set the route callback return code in *cbrc
int fskit_route_call_create( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
   return fskit_route_call( core, FSKIT_ROUTE_MATCH_CREATE, path, fent, dargs, cbrc );
}

// call the route to create a device node.  The requisite inode_data will be set in dargs on success.
// return 0 if a route was called, or -EPERM if there are no routes.
// set the route callback return code in *cbrc
int fskit_route_call_mknod( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
   return fskit_route_call( core, FSKIT_ROUTE_MATCH_MKNOD, path, fent, dargs, cbrc );
}

// call the route to make a directory.  The requisite inode_data and handle_data will be set in dargs on success.
// return 0 if a route was called, or -EPERM if there are no routes.
// set the route callback return code in *cbrc
int fskit_route_call_mkdir( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
   return fskit_route_call( core, FSKIT_ROUTE_MATCH_MKDIR, path, fent, dargs, cbrc );
}

// call the route to open a file or directory.  The requisite handle_data will be set in dargs on success.
// return 0 if a route was called, or -EPERM if there are no routes.
// set the route callback return code in *cbrc
int fskit_route_call_open( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
   return fskit_route_call( core, FSKIT_ROUTE_MATCH_OPEN, path, fent, dargs, cbrc );
}

// call the route to close a file or directory.
// return 0 if a route was called, or -EPERM if there are no routes.
// set the route callback return code in *cbrc
int fskit_route_call_close( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
   return fskit_route_call( core, FSKIT_ROUTE_MATCH_CLOSE, path, fent, dargs, cbrc );
}

// call the route to read a directory 
// return 0 if a route was called, or -EPERM if there are no routes.
// set the route callback return code in *cbrc
int fskit_route_call_readdir( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
   return fskit_route_call( core, FSKIT_ROUTE_MATCH_READDIR, path, fent, dargs, cbrc );
}

// call the route to read(). The requisite iobuf will be filled in on success.
// return 0 if a route was called, or -EPERM if there are no routes.
// set the route callback return code in *cbrc
int fskit_route_call_read( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
   return fskit_route_call( core, FSKIT_ROUTE_MATCH_READ, path, fent, dargs, cbrc );
}

// call the route to write().
// return 0 if a route was called, or -EPERM if there are no routes.
// set the route callback return code in *cbrc
int fskit_route_call_write( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
   return fskit_route_call( core, FSKIT_ROUTE_MATCH_WRITE, path, fent, dargs, cbrc );
}

// call the route to trunc().
// return 0 if a route was called, or -EPERM if there are no routes.
// set the route callback return code in *cbrc
int fskit_route_call_trunc( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
   return fskit_route_call( core, FSKIT_ROUTE_MATCH_TRUNC, path, fent, dargs, cbrc );
}

// call the route to unlink or rmdir.
// return 0 if a route was called, or -EPERM if there are no routes.
// set the route callback return code in *cbrc
int fskit_route_call_detach( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
   return fskit_route_call( core, FSKIT_ROUTE_MATCH_DETACH, path, fent, dargs, cbrc );
}

// call the route to stat
// return 0 if a route was called, or -EPERM if there are no routes.
// set the route callback return code in *cbrc
int fskit_route_call_stat( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
   return fskit_route_call( core, FSKIT_ROUTE_MATCH_STAT, path, fent, dargs, cbrc );
}

// call the route to sync
// return 0 if a route was called, or -EPERM if there are no routes.
// set the route callback return code in *cbrc
int fskit_route_call_sync( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
   return fskit_route_call( core, FSKIT_ROUTE_MATCH_SYNC, path, fent, dargs, cbrc );
}

// initialize a path route 
// return 0 on success, negative on error
static int fskit_path_route_init( struct fskit_path_route* route, char const* regex_str, int consistency_discipline, int route_type, union fskit_route_method method ) {
   
   int rc = 0;
   memset( route, 0, sizeof(struct fskit_path_route) );
   
   rc = regcomp( &route->path_regex, regex_str, REG_EXTENDED | REG_NEWLINE );
   if( rc != 0 ) {
      
      fskit_error("regcomp('%s') rc = %d\n", regex_str, rc );
      return -EINVAL;
   }
   
   route->path_regex_str = strdup( regex_str );
   if( route->path_regex_str == NULL ) {
      
      regfree( &route->path_regex );
      return -ENOMEM;
   }
   
   route->num_expected_matches = fskit_num_expected_matches( regex_str );
   
   route->consistency_discipline = consistency_discipline;
   route->route_type = route_type;
   route->method = method;
   
   pthread_rwlock_init( &route->lock, NULL );
   
   return 0;
}

// free a path route 
int fskit_path_route_free( struct fskit_path_route* route ) {
   
   if( route->path_regex_str != NULL ) {
      safe_free( route->path_regex_str );
      route->path_regex_str = NULL;
      
      // NOTE: the regex is only set if the string is set 
      regfree( &route->path_regex );
      
      pthread_rwlock_destroy( &route->lock );
   }
   
   memset( route, 0, sizeof(struct fskit_path_route) );
   
   return 0;
}

// add a route to a route table, performing a shallow copy (i.e. don't free route after calling this method)
// return the index into the route table on success (this serves as the "handle" to the route)
static int fskit_path_route_add( fskit_route_table_t* route_table, struct fskit_path_route* route ) {
   
   fskit_route_table_t::iterator itr;
   fskit_route_list_t* route_list = NULL;
   
   // any routes for this type?
   itr = route_table->find( route->route_type );
   if( itr == route_table->end() ) {
      
      // map one in 
      (*route_table)[ route->route_type ] = fskit_route_list_t();
      route_list = &(*route_table)[ route->route_type ];
   }
   else {
      
      route_list = &itr->second;
   }
   
   // find an empty route slot to insert into...
   for( unsigned int i = 0; i < route_list->size(); i++ ) {
      
      if( !fskit_path_route_is_defined( &route_list->at(i) ) ) {
         
         // insert here
         (*route_list)[i] = *route;
         return i;
      }
   }
   
   // all routes are full; insert at the back
   route_list->push_back( *route );
   return route_list->size() - 1;
}


// remove a route from a route table, freeing and destroying it.
// return 0 on success 
// return -EINVAL if there is no such handle
static int fskit_path_route_erase( fskit_route_table_t* route_table, int route_type, int route_handle ) {
   
   fskit_route_table_t::iterator itr;
   fskit_route_list_t* route_list = NULL;
   
   // any routes for this type?
   itr = route_table->find( route_type );
   if( itr == route_table->end() ) {
      
      // can't possibly remove 
      return -EINVAL;
   }
   
   route_list = &itr->second;
   
   if( (unsigned)route_handle >= route_list->size() ) {
      
      // can't possibly exist 
      return -EINVAL;
   }
   
   // erase at this index 
   fskit_path_route_free( &route_list->at(route_handle) );
   return 0;
}

// declare a route 
// return >= 0 on success (this is the "route handle")
// return -EINVAL if we couldn't compile the regex 
// return -ENOMEM if out of memory 
static int fskit_path_route_decl( struct fskit_core* core, char const* route_regex, int route_type, union fskit_route_method method, int consistency_discipline ) {
   
   int rc = 0;
   struct fskit_path_route route;
   
   memset( &route, 0, sizeof(struct fskit_path_route) );
   
   rc = fskit_path_route_init( &route, route_regex, consistency_discipline, route_type, method );
   if( rc != 0 ) {
      
      return rc;
   }
   
   // atomically update route table
   fskit_core_route_wlock( core );
   
   rc = fskit_path_route_add( core->routes, &route );
   
   fskit_core_route_unlock( core );
   
   return rc;
}

// undeclare a route 
// return 0 on success
// return -EINVAL if it's a bad route handle
static int fskit_path_route_undecl( struct fskit_core* core, int route_type, int route_handle ) {
   
   int rc = 0;
   
   // atomically update route table
   fskit_core_route_wlock( core );
   
   rc = fskit_path_route_erase( core->routes, route_type, route_handle );
   
   fskit_core_route_unlock( core );
   
   return rc;
}

// declare a route for creating a file 
// return >= 0 on success (the route handle)
// return -EINVAL if we couldn't compile the regex 
// return -ENOMEM if out of memory 
int fskit_route_create( struct fskit_core* core, char const* route_regex, fskit_entry_route_create_callback_t create_cb, int consistency_discipline ) {
   
   union fskit_route_method method;
   method.create_cb = create_cb;
   
   return fskit_path_route_decl( core, route_regex, FSKIT_ROUTE_MATCH_CREATE, method, consistency_discipline );
}

// undeclare an existing route for creating a file 
// return 0 on success
// return -EINVAL if the route can't possibly exist.
int fskit_unroute_create( struct fskit_core* core, int route_handle ) {
   
   return fskit_path_route_undecl( core, FSKIT_ROUTE_MATCH_CREATE, route_handle );
}

// declare a route for creating a node 
// return >= 0 on success (the route handle)
// return -EINVAL if we couldn't compile the regex 
// return -ENOMEM if out of memory 
int fskit_route_mknod( struct fskit_core* core, char const* route_regex, fskit_entry_route_mknod_callback_t mknod_cb, int consistency_discipline ) {
   
   union fskit_route_method method;
   method.mknod_cb = mknod_cb;
   
   return fskit_path_route_decl( core, route_regex, FSKIT_ROUTE_MATCH_MKNOD, method, consistency_discipline );
}

// undeclare an existing route for creating a node
// return 0 on success
// return -EINVAL if the route can't possibly exist.
int fskit_unroute_mknod( struct fskit_core* core, int route_handle ) {
   
   return fskit_path_route_undecl( core, FSKIT_ROUTE_MATCH_MKNOD, route_handle );
}

// declare a route for making a directory
// return >= 0 on success (the route handle)
// return -EINVAL if we couldn't compile the regex 
// return -ENOMEM if out of memory 
int fskit_route_mkdir( struct fskit_core* core, char const* route_regex, fskit_entry_route_mkdir_callback_t mkdir_cb, int consistency_discipline ) {
   
   union fskit_route_method method;
   method.mkdir_cb = mkdir_cb;
   
   return fskit_path_route_decl( core, route_regex, FSKIT_ROUTE_MATCH_MKDIR, method, consistency_discipline );
}

// undeclare an existing route for making a directory
// return 0 on success
// return -EINVAL if the route can't possibly exist.
int fskit_unroute_mkdir( struct fskit_core* core, int route_handle ) {
   
   return fskit_path_route_undecl( core, FSKIT_ROUTE_MATCH_MKDIR, route_handle );
}

// declare a route for opening a file or directory
// return >= 0 on success (the route handle)
// return -EINVAL if we couldn't compile the regex 
// return -ENOMEM if out of memory 
int fskit_route_open( struct fskit_core* core, char const* route_regex, fskit_entry_route_open_callback_t open_cb, int consistency_discipline ) {
   
   union fskit_route_method method;
   method.open_cb = open_cb;
   
   return fskit_path_route_decl( core, route_regex, FSKIT_ROUTE_MATCH_OPEN, method, consistency_discipline );
}

// undeclare an existing route for opening a file
// return 0 on success
// return -EINVAL if the route can't possibly exist.
int fskit_unroute_open( struct fskit_core* core, int route_handle ) {
   
   return fskit_path_route_undecl( core, FSKIT_ROUTE_MATCH_OPEN, route_handle );
}

// declare a route for closing a file or directory
// return >= 0 on success (the route handle)
// return -EINVAL if we couldn't compile the regex 
// return -ENOMEM if out of memory 
int fskit_route_close( struct fskit_core* core, char const* route_regex, fskit_entry_route_close_callback_t close_cb, int consistency_discipline ) {
   
   union fskit_route_method method;
   method.close_cb = close_cb;
   
   return fskit_path_route_decl( core, route_regex, FSKIT_ROUTE_MATCH_CLOSE, method, consistency_discipline );
}

// undeclare an existing route for closing a file
// return 0 on success
// return -EINVAL if the route can't possibly exist.
int fskit_unroute_close( struct fskit_core* core, int route_handle ) {
   
   return fskit_path_route_undecl( core, FSKIT_ROUTE_MATCH_CLOSE, route_handle );
}

// declare a route for readdir'ing a directory
// return >= 0 on success (the route handle)
// return -EINVAL if we couldn't compile the regex 
// return -ENOMEM if out of memory 
int fskit_route_readdir( struct fskit_core* core, char const* route_regex, fskit_entry_route_readdir_callback_t readdir_cb, int consistency_discipline ) {
   
   union fskit_route_method method;
   method.readdir_cb = readdir_cb;
   
   return fskit_path_route_decl( core, route_regex, FSKIT_ROUTE_MATCH_READDIR, method, consistency_discipline );
}

// undeclare an existing route for reading a directory
// return 0 on success
// return -EINVAL if the route can't possibly exist.
int fskit_unroute_readdir( struct fskit_core* core, int route_handle ) {
   
   return fskit_path_route_undecl( core, FSKIT_ROUTE_MATCH_READDIR, route_handle );
}

// declare a route for reading a file
// return >= 0 on success (the route handle)
// return -EINVAL if we couldn't compile the regex 
// return -ENOMEM if out of memory 
int fskit_route_read( struct fskit_core* core, char const* route_regex, fskit_entry_route_io_callback_t io_cb, int consistency_discipline ) {
   
   union fskit_route_method method;
   method.io_cb = io_cb;
   
   return fskit_path_route_decl( core, route_regex, FSKIT_ROUTE_MATCH_READ, method, consistency_discipline );
}

// undeclare an existing route for reading a file 
// return 0 on success
// return -EINVAL if the route can't possibly exist.
int fskit_unroute_read( struct fskit_core* core, int route_handle ) {
   
   return fskit_path_route_undecl( core, FSKIT_ROUTE_MATCH_READ, route_handle );
}

// declare a route for writing a file
// return >= 0 on success (the route handle)
// return -EINVAL if we couldn't compile the regex 
// return -ENOMEM if out of memory 
int fskit_route_write( struct fskit_core* core, char const* route_regex, fskit_entry_route_io_callback_t io_cb, int consistency_discipline ) {
   
   union fskit_route_method method;
   method.io_cb = io_cb;
   
   return fskit_path_route_decl( core, route_regex, FSKIT_ROUTE_MATCH_WRITE, method, consistency_discipline );
}

// undeclare an existing route for writing a file 
// return 0 on success
// return -EINVAL if the route can't possibly exist.
int fskit_unroute_write( struct fskit_core* core, int route_handle ) {
   
   return fskit_path_route_undecl( core, FSKIT_ROUTE_MATCH_WRITE, route_handle );
}

// declare a route for truncating a file
// return >= 0 on success (the route handle)
// return -EINVAL if we couldn't compile the regex 
// return -ENOMEM if out of memory 
int fskit_route_trunc( struct fskit_core* core, char const* route_regex, fskit_entry_route_trunc_callback_t trunc_cb, int consistency_discipline ) {
   
   union fskit_route_method method;
   method.trunc_cb = trunc_cb;
   
   return fskit_path_route_decl( core, route_regex, FSKIT_ROUTE_MATCH_TRUNC, method, consistency_discipline );
}

// undeclare an existing route for truncating a file 
// return 0 on success
// return -EINVAL if the route can't possibly exist.
int fskit_unroute_trunc( struct fskit_core* core, int route_handle ) {
   
   return fskit_path_route_undecl( core, FSKIT_ROUTE_MATCH_TRUNC, route_handle );
}

// declare a route for detaching a file or directory
// return >= 0 on success (the route handle)
// return -EINVAL if we couldn't compile the regex 
// return -ENOMEM if out of memory 
int fskit_route_detach( struct fskit_core* core, char const* route_regex, fskit_entry_route_detach_callback_t detach_cb, int consistency_discipline ) {
   
   union fskit_route_method method;
   method.detach_cb = detach_cb;
   
   return fskit_path_route_decl( core, route_regex, FSKIT_ROUTE_MATCH_DETACH, method, consistency_discipline );
}

// undeclare an existing route for detaching a file or directory
// return 0 on success
// return -EINVAL if the route can't possibly exist.
int fskit_unroute_detach( struct fskit_core* core, int route_handle ) {
   
   return fskit_path_route_undecl( core, FSKIT_ROUTE_MATCH_DETACH, route_handle );
}

// declare a route for stating a file or directory
// return >= 0 on success (the route handle)
// return -EINVAL if we couldn't compile the regex 
// return -ENOMEM if out of memory 
int fskit_route_stat( struct fskit_core* core, char const* route_regex, fskit_entry_route_stat_callback_t stat_cb, int consistency_discipline ) {
   
   union fskit_route_method method;
   method.stat_cb = stat_cb;
   
   return fskit_path_route_decl( core, route_regex, FSKIT_ROUTE_MATCH_STAT, method, consistency_discipline );
}

// undeclare an existing route for stating a file or directory
// return 0 on success
// return -EINVAL if the route can't possibly exist.
int fskit_unroute_stat( struct fskit_core* core, int route_handle ) {
   
   return fskit_path_route_undecl( core, FSKIT_ROUTE_MATCH_STAT, route_handle );
}

// declare a route for syncing a file or directory
// return >= 0 on success (the route handle)
// return -EINVAL if we couldn't compile the regex 
// return -ENOMEM if out of memory 
int fskit_route_sync( struct fskit_core* core, char const* route_regex, fskit_entry_route_sync_callback_t sync_cb, int consistency_discipline ) {
   
   union fskit_route_method method;
   method.sync_cb = sync_cb;
   
   return fskit_path_route_decl( core, route_regex, FSKIT_ROUTE_MATCH_SYNC, method, consistency_discipline );
}

// undeclare an existing route for syncing a file or directory
// return 0 on success
// return -EINVAL if the route can't possibly exist.
int fskit_unroute_sync( struct fskit_core* core, int route_handle ) {
   
   return fskit_path_route_undecl( core, FSKIT_ROUTE_MATCH_SYNC, route_handle );
}

// set up dargs for create() 
int fskit_route_create_args( struct fskit_route_dispatch_args* dargs, mode_t mode ) {
   
   memset( dargs, 0, sizeof(struct fskit_route_dispatch_args) );
   
   dargs->mode = mode;
   
   return 0;
}

// set up dargs for mknod() 
int fskit_route_mknod_args( struct fskit_route_dispatch_args* dargs, mode_t mode, dev_t dev ) {
   
   memset( dargs, 0, sizeof(struct fskit_route_dispatch_args) );
   
   dargs->mode = mode;
   dargs->dev = dev;
   return 0;
}

// set up dargs for mkdir() 
int fskit_route_mkdir_args( struct fskit_route_dispatch_args* dargs, mode_t mode ) {
   
   memset( dargs, 0, sizeof(struct fskit_route_dispatch_args) );
   
   dargs->mode = mode;
   
   return 0;
}

// set up dargs for open() and opendir()
int fskit_route_open_args( struct fskit_route_dispatch_args* dargs, int flags ) {
   
   memset( dargs, 0, sizeof(struct fskit_route_dispatch_args) );
   
   dargs->flags = flags;
   return 0;
}

// set up dargs for close() and closedir()
int fskit_route_close_args( struct fskit_route_dispatch_args* dargs, void* handle_data ) {
   
   memset( dargs, 0, sizeof(struct fskit_route_dispatch_args) );
   
   dargs->handle_data = handle_data;
   return 0;
}

// set up dargs for readdir()
int fskit_route_readdir_args( struct fskit_route_dispatch_args* dargs, struct fskit_dir_entry** dents, uint64_t num_dents ) {
   
   memset( dargs, 0, sizeof(struct fskit_route_dispatch_args) );
   
   dargs->dents = dents;
   dargs->num_dents = num_dents;
   return 0;
}


// set up dargs for read() and write()
int fskit_route_io_args( struct fskit_route_dispatch_args* dargs, char* iobuf, size_t iolen, off_t iooff, void* handle_data ) {
   
   memset( dargs, 0, sizeof(struct fskit_route_dispatch_args) );
   
   dargs->iobuf = iobuf;
   dargs->iolen = iolen;
   dargs->iooff = iooff;
   dargs->handle_data = handle_data;
   
   return 0;
}

// set up dargs for trunc
int fskit_route_trunc_args( struct fskit_route_dispatch_args* dargs, off_t iooff, void* handle_data ) {
   
   memset( dargs, 0, sizeof(struct fskit_route_dispatch_args) );
   
   dargs->iooff = iooff;
   dargs->handle_data = handle_data;
   
   return 0;
}

// set up dargs for unlink() and rmdir()
int fskit_route_detach_args( struct fskit_route_dispatch_args* dargs, void* inode_data ) {
   
   memset( dargs, 0, sizeof(struct fskit_route_dispatch_args) );
   
   dargs->inode_data = inode_data;
   
   return 0;
}

// set up dargs for stat()
int fskit_route_stat_args( struct fskit_route_dispatch_args* dargs, struct stat* sb ) {
   
   memset( dargs, 0, sizeof(struct fskit_route_dispatch_args) );
   
   dargs->sb = sb;
   
   return 0;
}

// set up dargs for sync()
int fskit_route_sync_args( struct fskit_route_dispatch_args* dargs ) {
   
   memset( dargs, 0, sizeof(struct fskit_route_dispatch_args) );
   
   return 0;
}
