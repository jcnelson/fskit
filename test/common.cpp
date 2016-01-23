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

#include "common.h"

#include <vector>
using namespace std;

// type to type string
void fskit_type_to_string( int type, char type_buf[10] ) {

   switch( type ) {
      case FSKIT_ENTRY_TYPE_DEAD:
         strcpy(type_buf, "DEAD");
         break;

      case FSKIT_ENTRY_TYPE_FILE:
         strcpy(type_buf, "FILE");
         break;

      case FSKIT_ENTRY_TYPE_DIR:
         strcpy(type_buf, "DIR ");
         break;

      case FSKIT_ENTRY_TYPE_FIFO:
         strcpy(type_buf, "FIFO");
         break;

      case FSKIT_ENTRY_TYPE_SOCK:
         strcpy(type_buf, "SOCK");
         break;

      case FSKIT_ENTRY_TYPE_CHR:
         strcpy(type_buf, "CHAR");
         break;

      case FSKIT_ENTRY_TYPE_BLK:
         strcpy(type_buf, "BLCK");
         break;

      case FSKIT_ENTRY_TYPE_LNK:
         strcpy(type_buf, "LINK");
         break;

      default:
         strcpy(type_buf, "UNKN");
         break;
   }
}

// print out a tree to the given file stream
int fskit_print_tree( FILE* out, struct fskit_entry* root ) {

   struct fskit_entry* node = NULL;
   char* next_path = NULL;
   char type_str[10];
   int rc = 0;
   fskit_entry_set_itr itr;
   fskit_entry_set* child = NULL;

   vector< struct fskit_entry* > frontier;
   vector< char* > frontier_paths;

   frontier.push_back( root );
   frontier_paths.push_back( strdup("/") );

   while( frontier.size() > 0 ) {

      node = frontier[0];
      next_path = frontier_paths[0];

      frontier.erase( frontier.begin() );
      frontier_paths.erase( frontier_paths.begin() );

      fskit_type_to_string( fskit_entry_get_type( node ), type_str );

      /*
      fprintf( out, "%s: inode=%" PRIX64 " size=%jd mode=%o user=%" PRIu64 " group=%" PRIu64 " ctime=(%" PRId64 ".%" PRId32 ") mtime=(%" PRId64 ".%" PRId32 ") atime=(%" PRId64 ".%" PRId32 ") mem=%p \"%s\"\n",
                    type_str, node->file_id, node->size, node->mode, node->owner, node->group, node->ctime_sec, node->ctime_nsec, node->mtime_sec, node->mtime_nsec, node->atime_sec, node->atime_nsec, node, next_path );

      */
      if( fskit_entry_get_type( node ) == FSKIT_ENTRY_TYPE_DIR ) {

         fskit_entry_set* children = fskit_entry_get_children( node );
         if( children == NULL ) {
            fskit_error("ERR: children of %p == NULL\n", node );

            rc = -EINVAL;
            break;
         }

         // explore children
         for( child = fskit_entry_set_begin( &itr, children ); child != NULL; child = fskit_entry_set_next( &itr ) ) {

            char const* name = fskit_entry_set_name_at( child );
            struct fskit_entry* child_ent = fskit_entry_set_child_at( child );

            if( child == NULL ) {
               continue;
            }
            if( strcmp(".", name) == 0 || strcmp("..", name) == 0 ) {
               continue;
            }

            frontier.push_back( child_ent );
            frontier_paths.push_back( fskit_fullpath( next_path, name, NULL ) );
         }
      }

      free( next_path );
   }

   if( rc != 0 ) {

      for( unsigned int i = 0; i < frontier_paths.size(); i++ ) {

         if( frontier_paths.at(i) ) {
            free( frontier_paths.at(i) );
         }
      }

      frontier_paths.clear();
   }

   return rc;
}


// begin a functional test
int fskit_test_begin( struct fskit_core** core, void* test_data ) {

   int rc = 0;

   rc = fskit_library_init();
   if( rc != 0 ) {
      fskit_error("fskit_library_init rc = %d\n", rc );
      return rc;
   }

   *core = fskit_core_new();
   if( *core == NULL ) {
      return -ENOMEM;
   }

   rc = fskit_core_init( *core, test_data );
   if( rc != 0 ) {
      fskit_error("fskit_core_init rc = %d\n", rc );
   }

   return rc;
}


// end a functional test
int fskit_test_end( struct fskit_core* core, void** test_data ) {

   int rc = 0;

   rc = fskit_detach_all( core, "/" );
   if( rc != 0 ) {
      fskit_error("fskit_detach_all(\"/\") rc = %d\n", rc );
      return rc;
   }

   rc = fskit_core_destroy( core, test_data );
   if( rc != 0 ) {
      fskit_error("fskit_core_destroy rc = %d\n", rc );
      return rc;
   }

   rc = fskit_library_shutdown();
   if( rc != 0 ) {
      return rc;
   }

   free( core );

   return rc;
}

int fskit_test_mkdir_LR_recursive( struct fskit_core* core, char const* path, int depth ) {

   if( depth <= 0 ) {
      return 0;
   }

   fskit_debug("mkdir('%s')\n", path );
   
   int rc = fskit_mkdir( core, path, 0755, 0, 0 );
   if( rc != 0 ) {
      fskit_error("fskit_mkdir('%s') rc = %d\n", path, rc );
      return rc;
   }

   char* new_path_1 = fskit_fullpath( path, "L", NULL );
   char* new_path_2 = fskit_fullpath( path, "R", NULL );

   rc = fskit_test_mkdir_LR_recursive( core, new_path_1, depth - 1 );
   if( rc != 0 ) {
      fskit_error("fskit_test_mkdir_LR_recursive('%s') rc = %d\n", new_path_1, rc );

      free( new_path_1 );
      free( new_path_2 );
      return rc;
   }

   rc = fskit_test_mkdir_LR_recursive( core, new_path_2, depth - 1 );
   if( rc != 0 ) {
      fskit_error("fskit_test_mkdir_LR_recursive('%s') rc = %d\n", new_path_2, rc );

      free( new_path_1 );
      free( new_path_2 );
      return rc;
   }

   free( new_path_1 );
   free( new_path_2 );

   return 0;
}
