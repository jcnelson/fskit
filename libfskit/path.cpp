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

#include <fskit/path.h>
#include <fskit/util.h>

// join two paths, writing the result to dest if dest is not NULL.
// otherwise, allocate and return a buffer containing the joined paths.
char* fskit_fullpath( char const* root, char const* path, char* dest ) {

   char delim = 0;
   int path_off = 0;

   int len = strlen(path) + strlen(root) + 2;

   if( strlen(root) > 0 ) {
      size_t root_delim_off = strlen(root) - 1;
      if( root[root_delim_off] != '/' && path[0] != '/' ) {
         len++;
         delim = '/';
      }
      else if( root[root_delim_off] == '/' && path[0] == '/' ) {
         path_off = 1;
      }
   }

   if( dest == NULL ) {
      dest = CALLOC_LIST( char, len );
   }

   memset(dest, 0, len);

   strcpy( dest, root );
   if( delim != 0 ) {
      dest[strlen(dest)] = '/';
   }
   strcat( dest, path + path_off );

   return dest;
}

// get the directory name of a path.
// put it into dest if dest is not null.
// otherwise, allocate a buffer and return it.
char* fskit_dirname( char const* path, char* dest ) {
   if( dest == NULL ) {
      dest = CALLOC_LIST( char, strlen(path) + 1 );
   }

   // is this root?
   if( strlen(path) == 0 || strcmp( path, "/" ) == 0 ) {
      strcpy( dest, "/" );
      return dest;
   }

   int delim_i = strlen(path);
   if( path[delim_i] == '/' ) {
      delim_i--;
   }

   for( ; delim_i >= 0; delim_i-- ) {
      if( path[delim_i] == '/' ) {
         break;
      }
   }

   if( delim_i < 0 ) {
      delim_i = 0;
   }

   if( delim_i == 0 && path[0] == '/' ) {
      delim_i = 1;
   }

   strncpy( dest, path, delim_i );
   dest[delim_i+1] = '\0';
   return dest;
}

// determine how long the basename of a path is
size_t fskit_basename_len( char const* path ) {
   int delim_i = strlen(path) - 1;
   if( delim_i <= 0 ) {
      return 0;
   }
   if( path[delim_i] == '/' ) {
      // this path ends with '/', so skip over it if it isn't /
      if( delim_i > 0 ) {
         delim_i--;
      }
   }
   for( ; delim_i >= 0; delim_i-- ) {
      if( path[delim_i] == '/' ) {
         break;
      }
   }
   delim_i++;

   return strlen(path) - delim_i;
}


// get the basename of a (non-directory) path.
// put it into dest, if dest is not null.
// otherwise, allocate a buffer with it and return the buffer
char* fskit_basename( char const* path, char* dest ) {

   size_t len = fskit_basename_len( path );

   if( dest == NULL ) {
      dest = CALLOC_LIST( char, len + 1 );
   }
   else {
      memset( dest, 0, len + 1 );
   }

   strncpy( dest, path + strlen(path) - len, len );
   return dest;
}


// calculate the depth of a path
// the depth of / is 0
// the depth of /foo/bar/baz/ is 3
// the depth of /foo/bar/baz is also 3
// the paths must be normalized (no //), and not include ..
int fskit_depth( char const* path ) {
   
   int i = strlen(path) - 1;

   if( i <= 0 ) {
      return 0;
   }

   if( path[i] == '/' ) {
      i--;
   }

   int depth = 0;
   for( ; i >= 0; i-- ) {
      if( path[i] == '/' ) {
         depth++;
      }
   }

   return depth;
}


// chop up a path into its constituent names
// return 0 on success
// return -ENOMEM on OOM 
int fskit_path_split( char* path, char** ret_names ) {
   
   size_t path_len = strlen(path);
   size_t num_names = 0;
   char** names = NULL;
   size_t names_i = 0;
   
   // how many names?
   for( size_t i = 0; i < path_len; i++ ) {
      
      // skip to next non-'/'
      if( path[i] != '/' ) {
         
         num_names++;
         
         // skip this name; advance to next '/'
         for( ; i < path_len; i++ ) {
            
            if( path[i] == '/' ) {
               break;
            }
         }
      }
   }
   
   names = CALLOC_LIST( char*, num_names + 1 );
   if( names == NULL ) {
      
      return -ENOMEM;
   }
   
   for( size_t i = 0; i < path_len; i++ ) {
      
      if( path[i] == '/' ) {
         path[i] = 0;
      }
      else {
            
         // next name
         names[names_i] = &path[i];
         names_i++;
         
         // skip this name; advance to next '/' 
         for( ; i < path_len; i++ ) {

            if( path[i] == '/' ) {
               
               path[i] = 0;
               break;
            }
         }
      }
   }
   
   return 0;
}


// make sure paths don't end in /, unless they're root.
// NOTE: this modifies the argument
void fskit_sanitize_path( char* path ) {
   if( strcmp( path, "/" ) != 0 ) {
      size_t len = strlen(path);
      if( len > 0 ) {
         if( path[len-1] == '/' ) {
            path[len-1] = '\0';
         }
      }
   }
}

// run the eval function on cur_ent.
// prev_ent must be write-locked, in case cur_ent gets deleted.
// return the eval function's return code.
// if the eval function fails, both cur_ent and prev_ent will be unlocked
static int fskit_entry_ent_eval( struct fskit_entry* prev_ent, struct fskit_entry* cur_ent, int (*ent_eval)( struct fskit_entry*, void* ), void* cls ) {
   
   long name_hash = fskit_entry_name_hash( cur_ent->name );
   char* name_dup = strdup( cur_ent->name );

   int eval_rc = (*ent_eval)( cur_ent, cls );
   if( eval_rc != 0 ) {

      fskit_debug("ent_eval(%" PRIX64 " (%s)) rc = %d\n", cur_ent->file_id, name_dup, eval_rc );

      // cur_ent might not even exist anymore....
      if( cur_ent->type != FSKIT_ENTRY_TYPE_DEAD ) {
         
         fskit_entry_unlock( cur_ent );
         if( prev_ent != cur_ent && prev_ent != NULL ) {
            
            fskit_entry_unlock( prev_ent );
         }
      }
      else {
         fskit_safe_free( cur_ent );

         if( prev_ent != NULL ) {
            
            fskit_debug("Remove %s from %s\n", name_dup, prev_ent->name );
            fskit_entry_set_remove_hash( prev_ent->children, name_hash );
            fskit_entry_unlock( prev_ent );
         }
      }
   }

   fskit_safe_free( name_dup );

   return eval_rc;
}

// resolve an absolute path, running a given function on each entry as the path is walked
// returns the locked fskit_entry at the end of the path on success
struct fskit_entry* fskit_entry_resolve_path_cls( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, bool writelock, int* err, int (*ent_eval)( struct fskit_entry*, void* ), void* cls ) {

   // if this path ends in '/', then append a '.'
   char* fpath = NULL;
   if( strlen(path) == 0 ) {
      *err = -EINVAL;
      return NULL;
   }

   if( path[strlen(path)-1] == '/' ) {
      fpath = fskit_fullpath( path, ".", NULL );
   }
   else {
      fpath = strdup( path );

      if( fpath == NULL ) {
         *err = -ENOMEM;
         return NULL;
      }
   }

   char* tmp = NULL;

   char* name = strtok_r( fpath, "/", &tmp );
   while( name != NULL && strcmp(name, ".") == 0 ) {
      name = strtok_r( NULL, "/", &tmp );
   }

   // if name == NULL, then root was requested.
   struct fskit_entry* cur_ent = fskit_core_resolve_root( core, (writelock && name == NULL) );
   struct fskit_entry* prev_ent = NULL;

   if( cur_ent->link_count == 0 || cur_ent->type == FSKIT_ENTRY_TYPE_DEAD ) {
      // filesystem was nuked
      fskit_safe_free( fpath );
      fskit_entry_unlock( cur_ent );
      *err = -ENOENT;
      return NULL;
   }

   // run our evaluator on the root entry (which is already locked)
   if( ent_eval ) {
      
      int eval_rc = fskit_entry_ent_eval( prev_ent, cur_ent, ent_eval, cls );
      if( eval_rc != 0 ) {
         *err = eval_rc;
         fskit_safe_free( fpath );
         fskit_entry_unlock( cur_ent );
         return NULL;
      }
   }

   do {

       // if this isn't a directory, then invalid path
       if( name != NULL && cur_ent->type != FSKIT_ENTRY_TYPE_DIR ) {
         if( cur_ent->type != FSKIT_ENTRY_TYPE_DEAD ) {
            *err = -ENOTDIR;
         }
         else {
            *err = -ENOENT;
         }

         fskit_safe_free( fpath );
         fskit_entry_unlock( cur_ent );

         return NULL;
      }

      // do we have permission to search this directory?
      if( cur_ent->type == FSKIT_ENTRY_TYPE_DIR && !FSKIT_ENTRY_IS_DIR_SEARCHABLE( cur_ent->mode, cur_ent->owner, cur_ent->group, user, group ) ) {

         fskit_error("User %" PRIu64 " of group %" PRIu64 " cannot read directory %" PRIX64 " owned by %" PRIu64 " in group %" PRIu64 "\n",
                user, group, cur_ent->file_id, cur_ent->owner, cur_ent->group );

         // the appropriate read flag is not set
         *err = -EACCES;
         fskit_safe_free( fpath );
         fskit_entry_unlock( cur_ent );

         return NULL;
      }

      // NOTE: check this here, since we might just be resolving root
      if( name == NULL ) {
         break;
      }

      // resolve next name
      prev_ent = cur_ent;
      if( name != NULL ) {

         if( prev_ent->type != FSKIT_ENTRY_TYPE_DIR ) {

            // not a directory
            *err = -ENOTDIR;
            fskit_safe_free( fpath );
            fskit_entry_unlock( prev_ent );

            return NULL;
         }
         else {
            cur_ent = fskit_entry_set_find_name( prev_ent->children, name );
         }
      }
      else {
         // out of path
         break;
      }

      // NOTE: we can safely check deletion_in_progress, since it only gets written once (and while the parent is write-locked)
      if( cur_ent == NULL || cur_ent->deletion_in_progress || cur_ent->type == FSKIT_ENTRY_TYPE_DEAD ) {

         // not found
         *err = -ENOENT;
         fskit_safe_free( fpath );
         fskit_entry_unlock( prev_ent );

         return NULL;
      }
      else {

         // next path name
         name = strtok_r( NULL, "/", &tmp );
         while( name != NULL && strcmp(name, ".") == 0 ) {
            name = strtok_r( NULL, "/", &tmp );
         }

         // keep to the locking discipline
         if( writelock ) {
            fskit_entry_wlock( cur_ent );
         }
         else {
            fskit_entry_rlock( cur_ent );
         }

         // before unlocking the previous ent, run our evaluator (if we have one)
         if( ent_eval ) {
            
            int eval_rc = fskit_entry_ent_eval( prev_ent, cur_ent, ent_eval, cls );
            if( eval_rc != 0 ) {

               fskit_entry_unlock( cur_ent );
               fskit_entry_unlock( prev_ent );

               *err = eval_rc;
               fskit_safe_free( fpath );

               return NULL;
            }
         }

         if( cur_ent->link_count == 0 || cur_ent->type == FSKIT_ENTRY_TYPE_DEAD ) {
            // just got removed

            fskit_entry_unlock( cur_ent );
            fskit_entry_unlock( prev_ent );

            *err = -ENOENT;
            fskit_safe_free( fpath );

            return NULL;
         }

         fskit_entry_unlock( prev_ent );
      }
   } while( true );

   fskit_safe_free( fpath );
   if( name == NULL ) {
      // ran out of path
      *err = 0;

      /*
      // check readability
      if( !FSKIT_ENTRY_IS_READABLE( cur_ent->mode, cur_ent->owner, cur_ent->group, user, group ) ) {

         fskit_error("User %" PRIu64 " of group %" PRIu64 " cannot read file %" PRIX64 " owned by %" PRIu64 " in group %" PRIu64 "\n",
                user, group, cur_ent->file_id, cur_ent->owner, cur_ent->group );

         *err = -EACCES;
         fskit_entry_unlock( cur_ent );
         return NULL;
      }
      */
      
      return cur_ent;
   }
   else {
      // not a directory
      *err = -ENOTDIR;
      fskit_entry_unlock( cur_ent );
      return NULL;
   }
}

// resolve an absolute path.
// returns the locked fskit_entry at the end of the path on success
struct fskit_entry* fskit_entry_resolve_path( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, bool writelock, int* err ) {
   return fskit_entry_resolve_path_cls( core, path, user, group, writelock, err, NULL, NULL );
}


// start iterating on a path 
// return an iterator 
struct fskit_path_iterator fskit_path_begin( struct fskit_core* core, char const* path, bool writelock ) {
   
   struct fskit_entry* root = NULL;
   
   struct fskit_path_iterator ret;
   
   memset( &ret, 0, sizeof(struct fskit_path_iterator) );
   
   // find root 
   root = fskit_core_resolve_root( core, writelock );
   if( root == NULL ) {
      
      ret.rc = -ENOENT;
      return ret;
   }
   
   // is root dead?
   if( root->link_count == 0 || root->type == FSKIT_ENTRY_TYPE_DEAD ) {
      
      fskit_entry_unlock( root );
      ret.rc = -ENOENT;
      return ret;
   }
   
   ret.core = core;
   ret.path = path;
   ret.name_i = 0;
   ret.writelock = writelock;
   ret.cur_ent = root;
   ret.prev_ent = NULL;
   
   ret.name = (char*)ret.path;
   while( *ret.name != '\0' ) {
      
      if( *ret.name != '/' ) {
         break;
      }
      
      ret.name++;
      ret.name_i++;
   }
   
   return ret;
}


// are we at the end of the path, or can we continue?
bool fskit_path_end( struct fskit_path_iterator* itr ) {
   
   if( itr->rc != 0 ) {
      return true;
   }
   
   if( itr->cur_ent == NULL ) {
      return true;
   }
   
   if( itr->end_of_path ) {
      return true;
   }
   
   return false;
}


// advance the path iterator to the next entry in the path.
// set itr->rc to -ENOTDIR if we encounter a file before running out of path
// set itr->rc to -ENOMEM if we're OOM 
// set itr->rc to -ENOENT if the named entry does not exist in the filesystem
void fskit_path_next( struct fskit_path_iterator* itr ) {
   
   char* tmp = itr->name;
   char* name_candidate = NULL;
   
   size_t len = 0;
   size_t name_len = 0;
   char* next_name = NULL;
   
   if( itr->end_of_path ) {
      return;
   }
   
   if( itr->prev_ent != NULL ) {
      
      fskit_entry_unlock( itr->prev_ent );
   }
   
   itr->prev_ent = itr->cur_ent;
   itr->cur_ent = NULL;
   
   // what's the next name?  Skip '.'
   tmp = itr->name;
   len = 0;             // total characters traversed in searching for the next non-"." name
   name_len = 0;        // candidate name length
   
   while( true ) {
         
      name_candidate = tmp;
      name_len = 0;
      
      // next delimited name
      while( *tmp != '\0' ) {
         
         if( *tmp == '/' ) {
            break;
         }
         
         tmp++;
         len++;
         name_len++;
      }
      
      // advance tmp beyond the '/''s--it'll become the name we search for next
      while( *tmp != '\0' ) {
         
         if( *tmp != '/' ) {
            break;
         }
         
         tmp++;
         len++;
      }
      
      if( *tmp == '\0' || len == 0 || name_len == 0 || itr->prev_ent == NULL ) {
         
         // out of path 
         itr->end_of_path = true;
         break;
      }
      else {
         
         if( name_len == 1 && *name_candidate == '.' ) {
            
            // skip
            continue;
         }
         else {
            
            break;
         }
      }
   }
   
   // we're in trouble if we're not at the end of the path, and itr->prev_ent is not a directory 
   if( !itr->end_of_path && fskit_entry_get_type( itr->prev_ent ) != FSKIT_ENTRY_TYPE_DIR ) {
      
      // not a directory 
      itr->rc = -ENOTDIR;
      return;
   }
   
   if( itr->end_of_path ) {
      
      // end-of-path 
      itr->rc = 0;
      return;
   }
   
   next_name = CALLOC_LIST( char, name_len + 1 );
   if( next_name == NULL ) {
      
      // OOM!
      itr->rc = -ENOMEM;
      return;
   }
   
   strncpy( next_name, name_candidate, name_len );
   
   // advance name (and path length considered)
   itr->name = tmp;
   itr->name_i += len;
   
   // look up the next entry in prev_ent 
   itr->cur_ent = fskit_entry_set_find_name( itr->prev_ent->children, next_name );
   
   fskit_safe_free( next_name );
   
   if( itr->cur_ent == NULL ) {
      
      // not found 
      itr->rc = -ENOENT;
      return;
   }
   
   if( itr->writelock ) {
      
      fskit_entry_wlock( itr->cur_ent );
   }
   else {
      
      fskit_entry_rlock( itr->cur_ent );
   }
   
   // success!
   return;
}


// get an iterator's error code 
int fskit_path_iterator_error( struct fskit_path_iterator* itr ) {
   
   return itr->rc;
}

// get an iterator's current entry 
struct fskit_entry* fskit_path_iterator_entry( struct fskit_path_iterator* itr ) {
   
   return itr->cur_ent;
}

// get an iterator entry's parent 
struct fskit_entry* fskit_path_iterator_entry_parent( struct fskit_path_iterator* itr ) {
   
   return itr->prev_ent;
}


// "release" an iterator (i.e. unlock its entries)
void fskit_path_iterator_release( struct fskit_path_iterator* itr ) {
   
   if( itr->prev_ent != NULL ) {
      
      fskit_entry_unlock( itr->prev_ent );
   }
   
   if( itr->cur_ent != NULL ) {
      
      fskit_entry_unlock( itr->cur_ent );
   }
   
   itr->prev_ent = NULL;
   itr->cur_ent = NULL;
}


// what's the path we're on?
// return a malloc'ed copy of the path to this entry the iterator represents 
// return NULL on OOM, or if the iterator was not initialized
char* fskit_path_iterator_path( struct fskit_path_iterator* itr ) {
   
   if( itr->path == NULL || itr->name == NULL ) {
      return NULL;
   }
      
   char* ret = CALLOC_LIST( char, itr->name_i + 1 );
   if( ret == NULL ) {
      
      return NULL;
   }
   
   strncpy( ret, itr->path, itr->name_i );
   return ret;
}


// reference an fskit_entry 
// resolve it, increment its open count, unlock it, and return a pointer to it.
// this is meant to prevent the fskit_entry from getting freed, but without locking it.
// return the pointer on success
// return NULL on error, and set *rc to the error code (i.e. from resolving the path)
struct fskit_entry* fskit_entry_ref( struct fskit_core* core, char const* fs_path, int* rc ) {
   
   struct fskit_entry* fent = NULL;
   
   fent = fskit_entry_resolve_path( core, fs_path, 0, 0, true, rc );
   if( fent == NULL ) {
      
      return NULL;
   }
   
   fent->open_count++;
   fskit_entry_unlock( fent );
   
   return fent;
}

// reference a write-locked entry 
// always succeeds
int fskit_entry_ref_entry( struct fskit_entry* fent ) {
   fent->open_count++;
   return 0;
}

// unreference an fskit_entry 
// decrement the open counter, and optionally delete it if it is fully unreferenced.
// return 0 on success
// return negative on error (from the user-given detach route)
// NOTE: fent must *not* be locked!
int fskit_entry_unref( struct fskit_core* core, char const* fs_path, struct fskit_entry* fent ) {
   
   int rc = 0;
   
   fskit_entry_wlock( fent );
   
   fent->open_count--;
   
   if( fent->open_count <= 0 && fent->link_count <= 0 ) {
      
      // blow it away 
      rc = fskit_entry_try_destroy_and_free( core, fs_path, fent );

      if( rc < 0 ) {

         // some error occurred
         fskit_error("fskit_entry_try_destroy(%p) rc = %d\n", fent, rc );
         fskit_entry_unlock( fent );

         return rc;
      }
      else if( rc == 0 ) {

         // done with this entry--it's still exists
         fskit_entry_unlock( fent );
      }
      else {

         // destroyed and freed
         rc = 0;
      }
   }
   
   return rc;
}
