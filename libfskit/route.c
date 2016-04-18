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

#include <fskit/entry.h>
#include <fskit/route.h>
#include <fskit/util.h>

#include "fskit_private/private.h"

struct fskit_route_table_row {
   
   int route_type;
   fskit_route_list_t routes;
   
   // for rb tree
   struct fskit_route_table_row* left;
   struct fskit_route_table_row* right;
   char color;
};


SGLIB_DEFINE_VECTOR_FUNCTIONS( fskit_path_route_entry );

SGLIB_DEFINE_RBTREE_FUNCTIONS( fskit_route_table, left, right, color, FSKIT_ROUTE_TABLE_ROW_CMP );


// new empty route table 
fskit_route_table* fskit_route_table_new(void) {
   
   fskit_route_table* ret = CALLOC_LIST( fskit_route_table, 1 );
   if( ret == NULL ) {
      return NULL;
   }
   
   ret->route_type = -1;
   
   return ret;
}


// new route table row 
struct fskit_route_table_row* fskit_route_table_row_new( int route_type ) {
   
   struct fskit_route_table_row* row = CALLOC_LIST( struct fskit_route_table_row, 1 );
   if( row == NULL ) {
      return NULL;
   }
   
   row->route_type = route_type;
   sglib_fskit_path_route_entry_vector_init( &row->routes );
   
   return row;
}

// row length 
static unsigned long fskit_route_table_row_len( struct fskit_route_table_row* row ) {
   
   return sglib_fskit_path_route_entry_vector_size( &row->routes );
}


// row entry 
static struct fskit_path_route* fskit_route_table_row_at_ref( struct fskit_route_table_row* row, unsigned long i ) {
   
   return sglib_fskit_path_route_entry_vector_at( &row->routes, i );
}

// emplace entry 
static int fskit_route_table_row_emplace( struct fskit_route_table_row* row, struct fskit_path_route* route, unsigned long i ) {
   
   return sglib_fskit_path_route_entry_vector_set( &row->routes, route, i );
}

// append entry 
static int fskit_route_table_row_append( struct fskit_route_table_row* row, struct fskit_path_route* route ) {
   
   return sglib_fskit_path_route_entry_vector_push_back( &row->routes, route );
}

// start iterating over a route table
struct fskit_route_table_row* fskit_route_table_begin( fskit_route_table_itr* itr, fskit_route_table* route_table ) {
   
   return sglib_fskit_route_table_it_init_inorder( itr, route_table );
}

// get the next entry in a directory entry set 
struct fskit_route_table_row* fskit_route_table_next( fskit_route_table_itr* itr ) {
   
   return sglib_fskit_route_table_it_next( itr );
}

// free up a row, and all the routes it contains.
int fskit_route_table_row_free( struct fskit_route_table_row* row ) {
   
   if( row != NULL ) {
   
      for( unsigned long i = 0; i < fskit_route_table_row_len( row ); i++ ) {
         
         struct fskit_path_route* route = fskit_route_table_row_at_ref( row, i );
         if( route == NULL ) {
             continue;
         }
         
         fskit_path_route_free( route );
         fskit_safe_free( route );
      }
      
      sglib_fskit_path_route_entry_vector_free( &row->routes );
   }
   
   return 0;
}

// free up a whole table--all its rows and its routes
int fskit_route_table_free( fskit_route_table* route_table ) {
   
   fskit_route_table_itr itr;
   struct fskit_route_table_row* row = NULL;
   struct fskit_route_table_row* old_row = NULL;
   
   for( row = fskit_route_table_begin( &itr, route_table ); row != NULL; row = fskit_route_table_next( &itr ) ) {
      
      fskit_route_table_row_free( row );
   }
   
   for( row = fskit_route_table_begin( &itr, route_table ); row != NULL; ) {
      
      old_row = row;
      row = fskit_route_table_next( &itr );
      
      fskit_safe_free( old_row );
   }
   
   return 0;
}


// insert a route into the route table.  Puts the pointer only; does not duplicate the route (i.e. the table owns the route now).
// return a route ID on success (>= 0)
// return -ENOMEM on OOM 
int fskit_route_table_insert( fskit_route_table** route_table, int route_type, struct fskit_path_route* route ) {
   
   int rc = 0;
   int route_id = -1;
   struct fskit_route_table_row lookup;
   struct fskit_route_table_row* row = NULL;       // routes for this route type 
   
   memset( &lookup, 0, sizeof(struct fskit_route_table_row) );
   
   lookup.route_type = route_type;
   row = sglib_fskit_route_table_find_member( *route_table, &lookup );
   
   if( row == NULL ) {
      
      // no route row for this type exists.  make one 
      row = fskit_route_table_row_new( route_type );
      if( row == NULL ) {
         
         return -ENOMEM;
      }
      
      fskit_debug("Add new route table row %p for type %d\n", row, route_type );
      sglib_fskit_route_table_add( route_table, row );
   }
   
   // find an empty slot
   for( unsigned long i = 0; i < fskit_route_table_row_len( row ); i++ ) {
      
      if( fskit_route_table_row_at_ref( row, i ) == NULL ) {
         
         // emplace 
         fskit_route_table_row_emplace( row, route, i );
         route_id = (int)i;
         break;
      }
   }
   
   if( route_id < 0 ) {
      
      rc = fskit_route_table_row_append( row, route );
      if( rc < 0 ) {
         
         return -ENOMEM;
      }
      
      route_id = fskit_route_table_row_len( row ) - 1;      
   }
   
   fskit_debug("Add new route table row entry %p for type %d at %d\n", route, route_type, route_id );
   return route_id;
}


// get a row in the route table
// return NULL if not found 
struct fskit_route_table_row* fskit_route_table_get_row( fskit_route_table* route_table, int route_type ) {
   
   struct fskit_route_table_row lookup;
   struct fskit_route_table_row* routes = NULL;
   
   memset( &lookup, 0, sizeof(struct fskit_route_table_row) );
   
   lookup.route_type = route_type;
   return sglib_fskit_route_table_find_member( route_table, &lookup );
}


// find a route in the route table 
// return NULL if not found
struct fskit_path_route* fskit_route_table_find( fskit_route_table* route_table, int route_type, int route_id ) {
   
   struct fskit_route_table_row* row = NULL;
   struct fskit_path_route* route = NULL;
   
   row = fskit_route_table_get_row( route_table, route_type );
   if( row == NULL ) {
      return NULL;
   }
   
   if( route_id >= (signed)fskit_route_table_row_len( row ) ) {
      return NULL;
   }
   
   return fskit_route_table_row_at_ref( row, route_id );
}

// remove a route from a route table 
// return the removed route on success; NULL if there is no such route
struct fskit_path_route* fskit_route_table_remove( fskit_route_table** route_table, int route_type, int route_id ) {
   
   struct fskit_route_table_row* row = NULL;
   struct fskit_path_route* route = NULL;
   bool free_row = true;
   
   row = fskit_route_table_get_row( *route_table, route_type );
   if( row == NULL ) {
      return NULL;
   }
   
   if( route_id >= (signed)fskit_route_table_row_len( row ) ) {
      return NULL;
   }
   
   route = fskit_route_table_row_at_ref( row, route_id );
   fskit_route_table_row_emplace( row, NULL, route_id );
   
   // can we free this row?
   for( unsigned long i = 0; i < fskit_route_table_row_len( row ); i++ ) {
      
      if( fskit_route_table_row_at_ref( row, i ) != NULL ) {
         free_row = false;
         break;
      }
   }
   
   if( free_row ) {
      
      sglib_fskit_route_table_delete( route_table, row );
      fskit_route_table_row_free( row );
      fskit_safe_free( row );
   }
   
   return route;
}


// initialize route metadata
// the match group becomes the owner of matches
// return 0 on success
static int fskit_route_metadata_init( struct fskit_route_metadata* route_metadata, char* matched_path, int num_matches, char** matches ) {
   memset( route_metadata, 0, sizeof(struct fskit_route_metadata) );

   // shallow copy
   route_metadata->argc = num_matches;
   route_metadata->argv = matches;
   route_metadata->path = matched_path;

   return 0;
}


// free a match group
// return 0 on success
static int fskit_route_metadata_free( struct fskit_route_metadata* route_metadata ) {

   if( route_metadata->argv != NULL ) {

      for( int i = 0; i < route_metadata->argc; i++ ) {

         if( route_metadata->argv[i] != NULL ) {

            free( route_metadata->argv[i] );
            route_metadata->argv[i] = NULL;
         }
      }

      free( route_metadata->argv );
      route_metadata->argv = NULL;
   }


   if( route_metadata->path != NULL ) {

      fskit_safe_free( route_metadata->path );
      route_metadata->path = NULL;
   }
   
   if( route_metadata->name != NULL ) {
      
      fskit_safe_free( route_metadata->name );
      route_metadata->name = NULL;
   }

   memset( route_metadata, 0, sizeof(struct fskit_route_metadata) );

   return 0;
}


// how many expected matche groups in a regex?
// stupidly simple heuristic: count the number of unescaped open parenthesis
// can overestimate; this just gives an upper bound.
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
static int fskit_match_regex( struct fskit_route_metadata* route_metadata, struct fskit_path_route* route, char const* path ) {

   int rc = 0;
   regmatch_t* m = CALLOC_LIST( regmatch_t, route->num_expected_matches + 1 );
   size_t path_len = strlen(path);

   if( m == NULL ) {

      return -ENOMEM;
   }

   rc = regexec( &route->path_regex, path, route->num_expected_matches, m, 0 );

   if( rc != 0 ) {
      // no matches
      fskit_safe_free( m );
      return -ENOENT;
   }

   // sanity check
   if( m[0].rm_so < 0 || m[0].rm_eo < 0 ) {
      // no match
      fskit_safe_free( m );
      return -ENOENT;
   }

   // matched! whole path?
   if( (signed)path_len != m[0].rm_eo - m[0].rm_so ) {
      // didn't match the whole path
      fskit_debug("Matched only %d:%d of 0:%zu in '%s'\n", m[0].rm_so, m[0].rm_eo, path_len, path );
      fskit_safe_free( m );
      return -ENOENT;
   }

   char** argv = CALLOC_LIST( char*, route->num_expected_matches + 1 );
   if( argv == NULL ) {

      fskit_safe_free( m );
      return -ENOMEM;
   }

   char* path_dup = strdup( path );
   if( path_dup == NULL ) {

      FREE_LIST( argv );
      fskit_safe_free( m );
      return -ENOMEM;
   }

   // accumulate matches
   int i = 1;
   for( i = 1; i <= route->num_expected_matches && m[i].rm_so >= 0 && m[i].rm_eo >= 0; i++ ) {

      char* next_match = CALLOC_LIST( char, m[i].rm_eo - m[i].rm_so + 1 );
      if( next_match == NULL ) {

         FREE_LIST( argv );
         fskit_safe_free( argv );
         fskit_safe_free( m );
         return -ENOMEM;
      }

      strncpy( next_match, path + m[i].rm_so, m[i].rm_eo - m[i].rm_so );

      argv[i-1] = next_match;
   }

   // i is the number of args
   fskit_route_metadata_init( route_metadata, path_dup, i, argv );

   fskit_safe_free( m );
   return 0;
}


// is a route defined?
static bool fskit_path_route_is_defined( struct fskit_path_route* route ) {

   // a route is defined if it has a defined path regex
   return route->path_regex_str != NULL;
}

// start running a route's callback.
// enforce the consistency discipline by locking the route appropriately
static int fskit_route_enter( struct fskit_path_route* route, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs ) {

   int rc = 0;
   
   if( fent == NULL && !dargs->fent_absent ) {
       fskit_error("%s", "BUG: entry is NULL\n");
       exit(1);
   }
  
   // the fent must be ref'ed before the route is called 
   if( fent != NULL && route->route_type != FSKIT_ROUTE_MATCH_DETACH && route->route_type != FSKIT_ROUTE_MATCH_DESTROY && fent->open_count <= 0 && fent->link_count <= 0 ) {
      fskit_error("\n\nBUG: entry %p is not ref'ed (open = %d, link = %d)\n\n", fent, fent->open_count, fent->link_count);
      exit(1);
   }

   // enforce the consistency discipline for this route
   if( route->consistency_discipline == FSKIT_SEQUENTIAL ) {
      rc = pthread_rwlock_wrlock( &route->lock );
   }
   else if( route->consistency_discipline == FSKIT_CONCURRENT ) {
      rc = pthread_rwlock_rdlock( &route->lock );
   }
   else if( fent != NULL && route->consistency_discipline == FSKIT_INODE_SEQUENTIAL ) {
      rc = fskit_entry_wlock( fent );
   }
   else if( fent != NULL && route->consistency_discipline == FSKIT_INODE_CONCURRENT ) {
      rc = fskit_entry_rlock( fent );
   }
   
   if( rc != 0 ) {
      // indicates deadlock
      fskit_error("BUG: locking route %s with discipline %d rc = %d\n", route->path_regex_str, route->consistency_discipline, rc );
      return rc;
   }
   
   return 0;
}

// finish running a route's callback.
// clean up from enforcing the consistency discipline
static int fskit_route_leave( struct fskit_path_route* route, struct fskit_entry* fent ) {
   
   if( fent != NULL && (route->consistency_discipline == FSKIT_INODE_SEQUENTIAL || route->consistency_discipline == FSKIT_INODE_CONCURRENT) ) {
      fskit_entry_unlock( fent );
   }
   else if( route->consistency_discipline == FSKIT_SEQUENTIAL || route->consistency_discipline == FSKIT_CONCURRENT ) {
      pthread_rwlock_unlock( &route->lock );
   }
   
   return 0;
}

#define fskit_safe_dispatch( method, ... ) ((method) == NULL ? -ENOSYS : (*method)( __VA_ARGS__ ))

// dispatch a route
// return the result of the callback, or -ENOSYS if the callback is NULL
// fent *cannot* be locked--its lock status will be set through the route's consistency discipline
// however, fent must have a positive open count, so it won't disappear during the user-given route execution
static int fskit_route_dispatch( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_path_route* route, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs ) {

   int rc = 0;

   // enforce the consistency discipline
   rc = fskit_route_enter( route, fent, dargs );
   if( rc != 0 ) {
      fskit_error("fskit_route_enter(route %s) rc = %d\n", route->path_regex_str, rc );
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

         rc = fskit_safe_dispatch( route->method.create_cb, core, route_metadata, fent, dargs->mode, &dargs->inode_data, &dargs->handle_data );
         break;

      case FSKIT_ROUTE_MATCH_MKNOD:

         rc = fskit_safe_dispatch( route->method.mknod_cb, core, route_metadata, fent, dargs->mode, dargs->dev, &dargs->inode_data );
         break;

      case FSKIT_ROUTE_MATCH_MKDIR:

         rc = fskit_safe_dispatch( route->method.mkdir_cb, core, route_metadata, fent, dargs->mode, &dargs->inode_data );
         break;

      case FSKIT_ROUTE_MATCH_OPEN:

         rc = fskit_safe_dispatch( route->method.open_cb, core, route_metadata, fent, dargs->flags, &dargs->handle_data );
         break;

      case FSKIT_ROUTE_MATCH_READDIR:

         rc = fskit_safe_dispatch( route->method.readdir_cb, core, route_metadata, fent, dargs->dents, dargs->num_dents );
         break;

      case FSKIT_ROUTE_MATCH_READ:
      case FSKIT_ROUTE_MATCH_WRITE:

         rc = fskit_safe_dispatch( route->method.io_cb, core, route_metadata, fent, dargs->iobuf, dargs->iolen, dargs->iooff, dargs->handle_data );

         if( dargs->io_cont != NULL ) {
            // call the continuation within the context of the enforced consistency discipline
            (*dargs->io_cont)( core, fent, dargs->iooff, rc );
         }

         break;

      case FSKIT_ROUTE_MATCH_TRUNC:

         rc = fskit_safe_dispatch( route->method.trunc_cb, core, route_metadata, fent, dargs->iooff, dargs->handle_data );

         if( dargs->io_cont != NULL ) {
            // call the continuation within the context of the enforced consistency discipline
            (*dargs->io_cont)( core, fent, dargs->iooff, rc );
         }

         break;

      case FSKIT_ROUTE_MATCH_CLOSE:

         rc = fskit_safe_dispatch( route->method.close_cb, core, route_metadata, fent, dargs->handle_data );
         break;

      case FSKIT_ROUTE_MATCH_DETACH:

         rc = fskit_safe_dispatch( route->method.detach_cb, core, route_metadata, fent, dargs->inode_data );
         break;

      case FSKIT_ROUTE_MATCH_DESTROY:
         
         rc = fskit_safe_dispatch( route->method.destroy_cb, core, route_metadata, fent, dargs->inode_data );
         break;
         
      case FSKIT_ROUTE_MATCH_STAT:

         rc = fskit_safe_dispatch( route->method.stat_cb, core, route_metadata, fent, dargs->sb );
         break;

      case FSKIT_ROUTE_MATCH_SYNC:

         rc = fskit_safe_dispatch( route->method.sync_cb, core, route_metadata, fent );
         break;

      case FSKIT_ROUTE_MATCH_RENAME:
         
         rc = fskit_safe_dispatch( route->method.rename_cb, core, route_metadata, fent, dargs->new_path, dargs->dest );
         break;
         
      case FSKIT_ROUTE_MATCH_LINK:
         
         rc = fskit_safe_dispatch( route->method.link_cb, core, route_metadata, fent, dargs->new_path );
         break;
         
      case FSKIT_ROUTE_MATCH_GETXATTR:

         rc = fskit_safe_dispatch( route->method.getxattr_cb, core, route_metadata, fent, dargs->xattr_name, dargs->xattr_buf, dargs->xattr_buf_len );
         break;

      case FSKIT_ROUTE_MATCH_SETXATTR:
         
         rc = fskit_safe_dispatch( route->method.setxattr_cb, core, route_metadata, fent, dargs->xattr_name, dargs->xattr_value, dargs->xattr_value_len, dargs->xattr_flags );
         break;

      case FSKIT_ROUTE_MATCH_LISTXATTR:

         rc = fskit_safe_dispatch( route->method.listxattr_cb, core, route_metadata, fent, dargs->xattr_buf, dargs->xattr_buf_len );
         break;

      case FSKIT_ROUTE_MATCH_REMOVEXATTR:

         rc = fskit_safe_dispatch( route->method.removexattr_cb, core, route_metadata, fent, dargs->xattr_name );
         break;

      default:

         fskit_error("Invalid route dispatch code %d\n", route->route_type );
         rc = -EINVAL;
   }

   fskit_route_leave( route, fent );
   
   if( rc < 0 ) {
       fskit_error("fskit_safe_dispatch(%d) rc = %d\n", route->route_type, rc );
   }
   return rc;
}


// try to match a path and type to a route.
// we consider it "found" if we can match on a regex in the route table.
// return a pointer to the first matching route 
// return -ENOENT if no match
// NOTE: not thread-safe
static struct fskit_path_route* fskit_route_match( fskit_route_table* route_table, int route_type, char const* path, struct fskit_route_metadata* route_metadata ) {

   int rc = 0;
   struct fskit_path_route* route = NULL;
   
   struct fskit_route_table_row* row = fskit_route_table_get_row( route_table, route_type );
   
   if( row == NULL ) {
      return NULL;
   }
   
   for( unsigned long i = 0; i < fskit_route_table_row_len( row ); i++ ) {
      
      route = fskit_route_table_row_at_ref( row, i );
      
      if( !fskit_path_route_is_defined( route ) ) {
         continue;
      }
      
      // match?
      rc = fskit_match_regex( route_metadata, route, path );
      if( rc == 0 ) {
         
         // matched!
         return route;
      }
   }
   
   // no match
   fskit_debug("No match on route type %d on '%s'\n", route_type, path ); 
   return NULL;
}


// copy relevant route dispatch arguments to route metadata
// return 0 on success
static int fskit_route_metadata_populate( struct fskit_route_metadata* route_metadata, struct fskit_route_dispatch_args* dargs ) {
   
   if( dargs->name != NULL ) {
      route_metadata->name = strdup( dargs->name );
      if( route_metadata->name == NULL ) {
         return -ENOMEM;
      }
   }
   
   route_metadata->parent = dargs->parent;
   route_metadata->new_parent = dargs->new_parent;
   route_metadata->garbage_collect = dargs->garbage_collect;
   route_metadata->cls = dargs->cls;
   route_metadata->xattr_name = dargs->xattr_name;
   route_metadata->xattr_value = dargs->xattr_value;
   route_metadata->xattr_value_len = dargs->xattr_value_len;
   route_metadata->xattr_buf = dargs->xattr_buf;
   route_metadata->xattr_buf_len = dargs->xattr_buf_len;
   return 0;
}


// call a route
// return 0 on success, -EPERM if no route found
// place the callback status in *cbrc, if called.
// NOTE: fent *cannot* be locked--its lock status will be set through the route consistency discipline
int fskit_route_call( struct fskit_core* core, int route_type, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {

   int rc = 0;   
   struct fskit_route_metadata route_metadata;   
   struct fskit_path_route* route = NULL;

   memset( &route_metadata, 0, sizeof(struct fskit_route_metadata) );

   // stop routes from getting changed out from under us
   fskit_core_route_rlock( core );

   route = fskit_route_match( core->routes, route_type, path, &route_metadata );

   if( route == NULL ) {
      // no route found
      fskit_core_route_unlock( core );
      return -EPERM;
   }
   
   // found. propagate arguments 
   rc = fskit_route_metadata_populate( &route_metadata, dargs );
   if( rc != 0 ) {
      
      // failed for some reason
      fskit_core_route_unlock( core );
      fskit_route_metadata_free( &route_metadata );
      return -EPERM;
   }
   
   
   fskit_debug("Call route type %d (%d)\n", route->route_type, route_type );
               
   // dispatch
   *cbrc = fskit_route_dispatch( core, &route_metadata, route, fent, dargs );

   fskit_core_route_unlock( core );

   rc = fskit_route_metadata_free( &route_metadata );
   return rc;
}


// call the route to create a file.  The requisite inode_data and handle_data will be set in dargs on success.
// return 0 if a route was called, or -EPERM if there are no routes.
// set the route callback return code in *cbrc
// NOTE: fent *cannot* be locked--its lock status will be set through the route consistency discipline
int fskit_route_call_create( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
   return fskit_route_call( core, FSKIT_ROUTE_MATCH_CREATE, path, fent, dargs, cbrc );
}


// call the route to create a device node.  The requisite inode_data will be set in dargs on success.
// return 0 if a route was called, or -EPERM if there are no routes.
// set the route callback return code in *cbrc
// NOTE: fent *cannot* be locked--its lock status will be set through the route consistency discipline
int fskit_route_call_mknod( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
   return fskit_route_call( core, FSKIT_ROUTE_MATCH_MKNOD, path, fent, dargs, cbrc );
}


// call the route to make a directory.  The requisite inode_data and handle_data will be set in dargs on success.
// return 0 if a route was called, or -EPERM if there are no routes.
// set the route callback return code in *cbrc
// NOTE: fent *cannot* be locked--its lock status will be set through the route consistency discipline
int fskit_route_call_mkdir( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
   return fskit_route_call( core, FSKIT_ROUTE_MATCH_MKDIR, path, fent, dargs, cbrc );
}


// call the route to open a file or directory.  The requisite handle_data will be set in dargs on success.
// return 0 if a route was called, or -EPERM if there are no routes.
// set the route callback return code in *cbrc
// NOTE: fent *cannot* be locked--its lock status will be set through the route consistency discipline
int fskit_route_call_open( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
   return fskit_route_call( core, FSKIT_ROUTE_MATCH_OPEN, path, fent, dargs, cbrc );
}


// call the route to close a file or directory.
// return 0 if a route was called, or -EPERM if there are no routes.
// set the route callback return code in *cbrc
// NOTE: fent *cannot* be locked--its lock status will be set through the route consistency discipline
int fskit_route_call_close( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
   return fskit_route_call( core, FSKIT_ROUTE_MATCH_CLOSE, path, fent, dargs, cbrc );
}


// call the route to read a directory
// return 0 if a route was called, or -EPERM if there are no routes.
// set the route callback return code in *cbrc
// NOTE: fent *cannot* be locked--its lock status will be set through the route consistency discipline
int fskit_route_call_readdir( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
   return fskit_route_call( core, FSKIT_ROUTE_MATCH_READDIR, path, fent, dargs, cbrc );
}


// call the route to read(). The requisite iobuf will be filled in on success.
// return 0 if a route was called, or -EPERM if there are no routes.
// set the route callback return code in *cbrc
// NOTE: fent *cannot* be locked--its lock status will be set through the route consistency discipline
int fskit_route_call_read( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
   return fskit_route_call( core, FSKIT_ROUTE_MATCH_READ, path, fent, dargs, cbrc );
}


// call the route to write().
// return 0 if a route was called, or -EPERM if there are no routes.
// set the route callback return code in *cbrc
// NOTE: fent *cannot* be locked--its lock status will be set through the route consistency discipline
int fskit_route_call_write( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
   return fskit_route_call( core, FSKIT_ROUTE_MATCH_WRITE, path, fent, dargs, cbrc );
}


// call the route to trunc().
// return 0 if a route was called, or -EPERM if there are no routes.
// set the route callback return code in *cbrc
// NOTE: fent *cannot* be locked--its lock status will be set through the route consistency discipline
int fskit_route_call_trunc( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
   return fskit_route_call( core, FSKIT_ROUTE_MATCH_TRUNC, path, fent, dargs, cbrc );
}


// call the route to unlink or rmdir.
// return 0 if a route was called, or -EPERM if there are no routes.
// set the route callback return code in *cbrc
// NOTE: fent *cannot* be locked--its lock status will be set through the route consistency discipline
int fskit_route_call_detach( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
   return fskit_route_call( core, FSKIT_ROUTE_MATCH_DETACH, path, fent, dargs, cbrc );
}

// call the route to unlink or rmdir.
// return 0 if a route was called, or -EPERM if there are no routes.
// set the route callback return code in *cbrc
// NOTE: fent *cannot* be locked--its lock status will be set through the route consistency discipline
int fskit_route_call_destroy( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
   return fskit_route_call( core, FSKIT_ROUTE_MATCH_DESTROY, path, fent, dargs, cbrc );
}

// call the route to stat
// return 0 if a route was called, or -EPERM if there are no routes.
// set the route callback return code in *cbrc
// NOTE: fent *cannot* be locked--its lock status will be set through the route consistency discipline
int fskit_route_call_stat( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
   return fskit_route_call( core, FSKIT_ROUTE_MATCH_STAT, path, fent, dargs, cbrc );
}


// call the route to sync
// return 0 if a route was called, or -EPERM if there are no routes.
// set the route callback return code in *cbrc
// NOTE: fent *cannot* be locked--its lock status will be set through the route consistency discipline
int fskit_route_call_sync( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
   return fskit_route_call( core, FSKIT_ROUTE_MATCH_SYNC, path, fent, dargs, cbrc );
}


// call the route to rename 
// return 0 if a route was called, or -EPERM if there are no routes.
// set the route callback return code in *cbrc 
// NOTE: fent *cannot* be locked--its lock status will be set through the route consistency discipline 
// if dargs->dest exists, it will be write-locked
int fskit_route_call_rename( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
   return fskit_route_call( core, FSKIT_ROUTE_MATCH_RENAME, path, fent, dargs, cbrc );
}


// call the route to link 
// return 0 if a route was called, or -EPERM if there are no routes 
// set the route callback return code in *cbrc 
// NOTE: fent *cannot* be locked--its lock status will be set through the route consistency discipline 
int fskit_route_call_link( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
    return fskit_route_call( core, FSKIT_ROUTE_MATCH_LINK, path, fent, dargs, cbrc );
}


// call the route to getxattr
// return 0 if a route was called, or -EPERM if there are no routes 
// set the route callback return code in *cbrc
// NOTE: fent *cannot* be locked--its lock status will be set through the route consistency discipline 
int fskit_route_call_getxattr( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
    return fskit_route_call( core, FSKIT_ROUTE_MATCH_GETXATTR, path, fent, dargs, cbrc );
}


// call the route to listxattr
// return 0 if a route was called, or -EPERM if there are no routes 
// set the route callback return code in *cbrc
// NOTE: fent *cannot* be locked--its lock status will be set through the route consistency discipline 
int fskit_route_call_listxattr( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
    return fskit_route_call( core, FSKIT_ROUTE_MATCH_LISTXATTR, path, fent, dargs, cbrc );
}


// call the route to setxattr
// return 0 if a route was called, or -EPERM if there are no routes 
// set the route callback return code in *cbrc
// NOTE: fent *cannot* be locked--its lock status will be set through the route consistency discipline 
int fskit_route_call_setxattr( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
    return fskit_route_call( core, FSKIT_ROUTE_MATCH_SETXATTR, path, fent, dargs, cbrc );
}


// call the route to removexattr
// return 0 if a route was called, or -EPERM if there are no routes 
// set the route callback return code in *cbrc
// NOTE: fent *cannot* be locked--its lock status will be set through the route consistency discipline 
int fskit_route_call_removexattr( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc ) {
    return fskit_route_call( core, FSKIT_ROUTE_MATCH_REMOVEXATTR, path, fent, dargs, cbrc );
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
      fskit_safe_free( route->path_regex_str );
      route->path_regex_str = NULL;

      // NOTE: the regex is only set if the string is set
      regfree( &route->path_regex );

      pthread_rwlock_destroy( &route->lock );
   }

   memset( route, 0, sizeof(struct fskit_path_route) );

   return 0;
}


// remove all routes from a given route table, freeing them and destroying them 
// return 0 on success
// return -EINVAL if there are no routes defined for this type
static int fskit_path_route_erase_all( fskit_route_table** route_table, int route_type ) {
   
   struct fskit_route_table_row* row = fskit_route_table_get_row( *route_table, route_type );
   if( row == NULL ) {
      return -EINVAL;
   }
   
   // clear it out 
   sglib_fskit_route_table_delete( route_table, row );
   fskit_route_table_row_free( row );
   fskit_safe_free( row );
   
   return 0;
}

// declare a route
// return >= 0 on success (this is the "route handle")
// return -EINVAL if we couldn't compile the regex
// return -ENOMEM if out of memory
static int fskit_path_route_decl( struct fskit_core* core, char const* route_regex, int route_type, union fskit_route_method method, int consistency_discipline ) {

   int rc = 0;
   struct fskit_path_route* route = CALLOC_LIST( struct fskit_path_route, 1 );
   if( route == NULL ) {
      return -ENOMEM;
   }

   rc = fskit_path_route_init( route, route_regex, consistency_discipline, route_type, method );
   if( rc != 0 ) {
   
      fskit_safe_free( route );
      return rc;
   }

   // atomically update route table
   fskit_core_route_wlock( core );

   rc = fskit_route_table_insert( &core->routes, route_type, route );

   fskit_core_route_unlock( core );

   return rc;
}

// undeclare a route
// return 0 on success
// return -EINVAL if it's a bad route handle
static int fskit_path_route_undecl( struct fskit_core* core, int route_type, int route_handle ) {

   int rc = 0;

   struct fskit_path_route* route = NULL;

   // atomically update route table
   fskit_core_route_wlock( core );

   route = fskit_route_table_remove( &core->routes, route_type, route_handle );

   fskit_core_route_unlock( core );
   
   if( route == NULL ) {
      return -EINVAL;
   }
   else {
      
      // destroy 
      fskit_path_route_free( route );
      fskit_safe_free( route );
   }
   
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

// declare a route for destroying a file or directory
// return >= 0 on success (the route handle)
// return -EINVAL if we couldn't compile the regex
// return -ENOMEM if out of memory
int fskit_route_destroy( struct fskit_core* core, char const* route_regex, fskit_entry_route_destroy_callback_t destroy_cb, int consistency_discipline ) {

   union fskit_route_method method;
   method.destroy_cb = destroy_cb;

   return fskit_path_route_decl( core, route_regex, FSKIT_ROUTE_MATCH_DESTROY, method, consistency_discipline );
}

// undeclare an existing route for detaching a file or directory
// return 0 on success
// return -EINVAL if the route can't possibly exist.
int fskit_unroute_destroy( struct fskit_core* core, int route_handle ) {

   return fskit_path_route_undecl( core, FSKIT_ROUTE_MATCH_DESTROY, route_handle );
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


// declare a route for renaming a file or directory 
// Note that for renames, the consistency *must be* FSKIT_SEQUENTIAL or FSKIT_CONCURRENT, since both the source and dest inodes will be write-locked
// return >= 0 on success (the route handle)
// return -EINVAL if we couldn't compile the regex 
// return -EINVAL if consistency_discipline is not supported
// return -ENOMEM if out of memory 
int fskit_route_rename( struct fskit_core* core, char const* route_regex, fskit_entry_route_rename_callback_t rename_cb, int consistency_discipline ) {
   
   union fskit_route_method method;
   method.rename_cb = rename_cb;
   
   if( consistency_discipline != FSKIT_CONCURRENT && consistency_discipline != FSKIT_SEQUENTIAL ) {
      return -EINVAL;
   }
   
   return fskit_path_route_decl( core, route_regex, FSKIT_ROUTE_MATCH_RENAME, method, consistency_discipline );
}


// undeclare a route for renaming a file or directory 
// return 0 on success
// return -EINVAL if the route can't possibly exist
int fskit_unroute_rename( struct fskit_core* core, int route_handle ) {
   
   return fskit_path_route_undecl( core, FSKIT_ROUTE_MATCH_RENAME, route_handle );
}


// declare a route for linking a file into a new directory 
// return >=0 on success (the route handle )
// return -EINVAL if consistency_discipline is not supported
// return -ENOMEM if out of memory 
int fskit_route_link( struct fskit_core* core, char const* route_regex, fskit_entry_route_link_callback_t link_cb, int consistency_discipline ) {
    
   union fskit_route_method method;
   method.link_cb = link_cb;
   
   return fskit_path_route_decl( core, route_regex, FSKIT_ROUTE_MATCH_LINK, method, consistency_discipline );
}


// undeclare a route for linking a file into a directory 
// return 0 on success 
// return -EINVAL if the route can't possibly exist 
int fskit_unroute_link( struct fskit_core* core, int route_handle ) {
    
   return fskit_path_route_undecl( core, FSKIT_ROUTE_MATCH_LINK, route_handle );
}


// declare a route for getting an xattr 
// return >=0 on success (the route handle)
// return -EINVAL if consistency discipline is not supported
// return -ENOMEM if out of memory 
int fskit_route_getxattr( struct fskit_core* core, char const* route_regex, fskit_entry_route_getxattr_callback_t getxattr_cb, int consistency_discipline ) {

   union fskit_route_method method;
   method.getxattr_cb = getxattr_cb;

   return fskit_path_route_decl( core, route_regex, FSKIT_ROUTE_MATCH_GETXATTR, method, consistency_discipline );
}

// undeclare a route for getting an xattr
// return 0 on success
// return -EINVAL if the route can't possibly exist 
int fskit_unroute_getxattr( struct fskit_core* core, int route_handle ) {

   return fskit_path_route_undecl( core, FSKIT_ROUTE_MATCH_GETXATTR, route_handle );
}

// declare a route for listing an xattr 
// return >=0 on success (the route handle)
// return -EINVAL if consistency discipline is not supported
// return -ENOMEM if out of memory 
int fskit_route_listxattr( struct fskit_core* core, char const* route_regex, fskit_entry_route_listxattr_callback_t listxattr_cb, int consistency_discipline ) {

   union fskit_route_method method;
   method.listxattr_cb = listxattr_cb;

   return fskit_path_route_decl( core, route_regex, FSKIT_ROUTE_MATCH_LISTXATTR, method, consistency_discipline );
}

// undeclare a route for listing xattrs
// return 0 on success
// return -EINVAL if the route can't possibly exist 
int fskit_unroute_listxattr( struct fskit_core* core, int route_handle ) {

   return fskit_path_route_undecl( core, FSKIT_ROUTE_MATCH_LISTXATTR, route_handle );
}


// declare a route for setting an xattr 
// return >=0 on success (the route handle)
// return -EINVAL if consistency discipline is not supported
// return -ENOMEM if out of memory 
int fskit_route_setxattr( struct fskit_core* core, char const* route_regex, fskit_entry_route_setxattr_callback_t setxattr_cb, int consistency_discipline ) {

   union fskit_route_method method;
   method.setxattr_cb = setxattr_cb;

   return fskit_path_route_decl( core, route_regex, FSKIT_ROUTE_MATCH_SETXATTR, method, consistency_discipline );
}

// undeclare a route for setting an xattr
// return 0 on success
// return -EINVAL if the route can't possibly exist 
int fskit_unroute_setxattr( struct fskit_core* core, int route_handle ) {

   return fskit_path_route_undecl( core, FSKIT_ROUTE_MATCH_SETXATTR, route_handle );
}


// declare a route for removing an xattr 
// return >=0 on success (the route handle)
// return -EINVAL if consistency discipline is not supported
// return -ENOMEM if out of memory 
int fskit_route_removexattr( struct fskit_core* core, char const* route_regex, fskit_entry_route_removexattr_callback_t removexattr_cb, int consistency_discipline ) {

   union fskit_route_method method;
   method.removexattr_cb = removexattr_cb;

   return fskit_path_route_decl( core, route_regex, FSKIT_ROUTE_MATCH_REMOVEXATTR, method, consistency_discipline );
}

// undeclare a route for listing xattrs
// return 0 on success
// return -EINVAL if the route can't possibly exist 
int fskit_unroute_removexattr( struct fskit_core* core, int route_handle ) {

   return fskit_path_route_undecl( core, FSKIT_ROUTE_MATCH_REMOVEXATTR, route_handle );
}


// undeclare all routes 
// return 0 on success
int fskit_unroute_all( struct fskit_core* core ) {
   
   int rc = 0;
   
   // atomically update route table
   fskit_core_route_wlock( core );

   for( int i = 0; i < FSKIT_ROUTE_NUM_ROUTE_TYPES; i++ ) {
      
      fskit_path_route_erase_all( &core->routes, i );
   }
   
   fskit_core_route_unlock( core );

   return rc;
}

// set up dargs for create()
int fskit_route_create_args( struct fskit_route_dispatch_args* dargs, struct fskit_entry* parent, char const* name, mode_t mode, void* cls ) {

   memset( dargs, 0, sizeof(struct fskit_route_dispatch_args) );
   
   dargs->name = name;
   dargs->parent = parent;
   dargs->mode = mode;
   dargs->cls = cls;

   return 0;
}

// set up dargs for mknod()
int fskit_route_mknod_args( struct fskit_route_dispatch_args* dargs, struct fskit_entry* parent, char const* name, mode_t mode, dev_t dev, void* cls ) {

   memset( dargs, 0, sizeof(struct fskit_route_dispatch_args) );

   dargs->name = name;
   dargs->parent = parent;
   dargs->mode = mode;
   dargs->dev = dev;
   dargs->cls = cls;

   return 0;
}

// set up dargs for mkdir()
int fskit_route_mkdir_args( struct fskit_route_dispatch_args* dargs, struct fskit_entry* parent, char const* name, mode_t mode, void* cls ) {

   memset( dargs, 0, sizeof(struct fskit_route_dispatch_args) );

   dargs->name = name;
   dargs->parent = parent;
   dargs->mode = mode;
   dargs->cls = cls;

   return 0;
}

// set up dargs for open() and opendir()
int fskit_route_open_args( struct fskit_route_dispatch_args* dargs, char const* name, int flags ) {

   memset( dargs, 0, sizeof(struct fskit_route_dispatch_args) );

   dargs->name = name;
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
int fskit_route_readdir_args( struct fskit_route_dispatch_args* dargs, char const* name, struct fskit_dir_entry** dents, uint64_t num_dents ) {

   memset( dargs, 0, sizeof(struct fskit_route_dispatch_args) );

   dargs->name = name;
   dargs->dents = dents;
   dargs->num_dents = num_dents;
   return 0;
}


// set up dargs for read() and write()
int fskit_route_io_args( struct fskit_route_dispatch_args* dargs, char* iobuf, size_t iolen, off_t iooff, void* handle_data, fskit_route_io_continuation io_cont ) {

   memset( dargs, 0, sizeof(struct fskit_route_dispatch_args) );

   dargs->iobuf = iobuf;
   dargs->iolen = iolen;
   dargs->iooff = iooff;
   dargs->handle_data = handle_data;
   dargs->io_cont = io_cont;

   return 0;
}

// set up dargs for trunc
int fskit_route_trunc_args( struct fskit_route_dispatch_args* dargs, char const* name, off_t iooff, void* handle_data, fskit_route_io_continuation io_cont ) {

   memset( dargs, 0, sizeof(struct fskit_route_dispatch_args) );

   dargs->name = name;
   dargs->iooff = iooff;
   dargs->handle_data = handle_data;
   dargs->io_cont = io_cont;

   return 0;
}

// set up dargs for unlink(), where we only decrement the link count
int fskit_route_detach_args( struct fskit_route_dispatch_args* dargs, struct fskit_entry* parent, char const* name, bool garbage_collect, void* inode_data ) {

   memset( dargs, 0, sizeof(struct fskit_route_dispatch_args) );

   dargs->name = name;
   dargs->parent = parent;
   dargs->garbage_collect = garbage_collect;
   dargs->inode_data = inode_data;

   return 0;
}

// set up dargs for unlink(), rmdir(), and general destruction
int fskit_route_destroy_args( struct fskit_route_dispatch_args* dargs, struct fskit_entry* parent, char const* name, void* inode_data ) {

   memset( dargs, 0, sizeof(struct fskit_route_dispatch_args) );

   dargs->name = name;
   dargs->parent = parent;
   dargs->inode_data = inode_data;

   return 0;
}

// set up dargs for stat()
int fskit_route_stat_args( struct fskit_route_dispatch_args* dargs, char const* name, struct stat* sb, bool fent_absent ) {

   memset( dargs, 0, sizeof(struct fskit_route_dispatch_args) );

   dargs->name = name;
   dargs->sb = sb;
   dargs->fent_absent = fent_absent;

   return 0;
}

// set up dargs for sync()
int fskit_route_sync_args( struct fskit_route_dispatch_args* dargs ) {

   memset( dargs, 0, sizeof(struct fskit_route_dispatch_args) );

   return 0;
}

// set up dargs for rename()
int fskit_route_rename_args( struct fskit_route_dispatch_args* dargs, struct fskit_entry* old_parent, char const* old_name, char const* new_path, struct fskit_entry* new_parent, struct fskit_entry* dest ) {
   
   memset( dargs, 0, sizeof(struct fskit_route_dispatch_args) );
   
   dargs->name = old_name,
   dargs->parent = old_parent;
   dargs->new_path = new_path;
   dargs->dest = dest;
   dargs->new_parent = new_parent;
   
   return 0;
}

// set up dargs for link()
int fskit_route_link_args( struct fskit_route_dispatch_args* dargs, char const* name, char const* new_path, struct fskit_entry* new_parent ) {
   
   memset( dargs, 0, sizeof(struct fskit_route_dispatch_args) );
   
   dargs->name = name;
   dargs->new_parent = new_parent;
   dargs->new_path = new_path;
   
   return 0;
}

// set up dargs for getxattr()
int fskit_route_getxattr_args( struct fskit_route_dispatch_args* dargs, char const* xattr_name, char* xattr_buf, size_t xattr_buf_len ) {

   memset( dargs, 0, sizeof(struct fskit_route_dispatch_args) );

   dargs->xattr_name = xattr_name;
   dargs->xattr_buf = xattr_buf;
   dargs->xattr_buf_len = xattr_buf_len;
   return 0;
}

// set up dargs for setxattr 
int fskit_route_setxattr_args( struct fskit_route_dispatch_args* dargs, char const* xattr_name, char const* xattr_value, size_t xattr_value_len, int flags ) {
   
   memset( dargs, 0, sizeof(struct fskit_route_dispatch_args) );

   dargs->xattr_name = xattr_name;
   dargs->xattr_value = xattr_value;
   dargs->xattr_value_len = xattr_value_len;
   dargs->xattr_flags = flags;
   return 0;
}

// set up dargs for listxattr 
int fskit_route_listxattr_args( struct fskit_route_dispatch_args* dargs, char* xattr_buf, size_t xattr_buf_len ) {

   memset( dargs, 0, sizeof(struct fskit_route_dispatch_args));

   dargs->xattr_buf = xattr_buf;
   dargs->xattr_buf_len = xattr_buf_len;
   return 0;
}

// set up dargs for removexattr 
int fskit_route_removexattr_args( struct fskit_route_dispatch_args* dargs, char const* xattr_name ) {

   memset( dargs, 0, sizeof(struct fskit_route_dispatch_args));
   dargs->xattr_name = xattr_name;
   return 0;
}

// get the route metadata path 
char* fskit_route_metadata_get_path( struct fskit_route_metadata* route_metadata ) {
   return route_metadata->path;
}

// get the route metadata name 
char* fskit_route_metadata_get_name( struct fskit_route_metadata* route_metadata ) {
   return route_metadata->name;
}

// get caller-given closure 
void* fskit_route_metadata_get_cls( struct fskit_route_metadata* route_metadata ) {
   return route_metadata->cls;
}

// get the number of match groups 
int fskit_route_metadata_num_match_groups( struct fskit_route_metadata* route_metadata ) {
   return route_metadata->argc;
}

// get the match groups (null-terminated list of char*)
char** fskit_route_metadata_get_match_groups( struct fskit_route_metadata* route_metadata ) {
   return route_metadata->argv;
}

// get the parent of the matched entry (only valid for creat(), mknod(), mkdir(), and rename())
struct fskit_entry* fskit_route_metadata_get_parent( struct fskit_route_metadata* route_metadata ) {
   return route_metadata->parent;
}

// get the parent of the entry to which we will attach the new entry (only valid for rename())
struct fskit_entry* fskit_route_metadata_get_new_parent( struct fskit_route_metadata* route_metadata ) {
   return route_metadata->new_parent;
}

// get the new path for the entry (valid for rename() only)
char* fskit_route_metadata_get_new_path( struct fskit_route_metadata* route_metadata ) {
   return route_metadata->new_path;
}

// determine if this node is being garbage-collected (for unlink() and rmdir() only)
bool fskit_route_metadata_is_garbage_collected( struct fskit_route_metadata* route_metadata ) {
   return route_metadata->garbage_collect;
}

// get the xattr name 
char const* fskit_route_metadata_get_xattr_name( struct fskit_route_metadata* route_metadata ) {
   return route_metadata->xattr_name;
}

// get the xattr value and len 
char const* fskit_route_metadata_get_xattr_value( struct fskit_route_metadata* route_metadata, size_t* len ) {
   *len = route_metadata->xattr_value_len;
   return route_metadata->xattr_value;
}

// get the xattr value buffer and len 
char* fskit_route_metadata_get_xattr_buf( struct fskit_route_metadata* route_metadata, size_t* len ) {
   *len = route_metadata->xattr_buf_len;
   return route_metadata->xattr_buf;
}

