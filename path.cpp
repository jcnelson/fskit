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

#include "path.h"
#include "util.h"

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
         safe_free( cur_ent );
         
         if( prev_ent ) {
            fskit_debug("Remove %s from %s\n", name_dup, prev_ent->name );
            fskit_entry_set_remove_hash( prev_ent->children, name_hash );
            fskit_entry_unlock( prev_ent );
         }
      }
   }
   
   safe_free( name_dup );
   
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
      safe_free( fpath );
      fskit_entry_unlock( cur_ent );
      *err = -ENOENT;
      return NULL;
   }

   // run our evaluator on the root entry (which is already locked)
   if( ent_eval ) {
      int eval_rc = fskit_entry_ent_eval( prev_ent, cur_ent, ent_eval, cls );
      if( eval_rc != 0 ) {
         *err = eval_rc;
         safe_free( fpath );
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

         safe_free( fpath );
         fskit_entry_unlock( cur_ent );

         return NULL;
      }

      // do we have permission to search this directory?
      if( cur_ent->type == FSKIT_ENTRY_TYPE_DIR && !FSKIT_ENTRY_IS_DIR_READABLE( cur_ent->mode, cur_ent->owner, cur_ent->group, user, group ) ) {
         
         fskit_error("User %" PRIu64 " of group %" PRIu64 " cannot read directory %" PRIX64 " owned by %" PRIu64 " in group %" PRIu64 "\n",
                user, group, cur_ent->file_id, cur_ent->owner, cur_ent->group );
         
         // the appropriate read flag is not set
         *err = -EACCES;
         safe_free( fpath );
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
            safe_free( fpath );
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
      if( cur_ent == NULL || cur_ent->deletion_in_progress ) {
         
         // not found
         *err = -ENOENT;
         safe_free( fpath );
         fskit_entry_unlock( prev_ent );
         
         return NULL;
      }
      else {
         
         // next path name
         name = strtok_r( NULL, "/", &tmp );
         while( name != NULL && strcmp(name, ".") == 0 ) {
            name = strtok_r( NULL, "/", &tmp );
         }
         
         // If this is the last step of the path,
         // do a write lock if requested
         if( name == NULL && writelock ) {
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
               safe_free( fpath );
               
               return NULL;
            }
         }
         
         if( cur_ent->link_count == 0 || cur_ent->type == FSKIT_ENTRY_TYPE_DEAD ) {
            // just got removed
            
            fskit_entry_unlock( cur_ent );
            fskit_entry_unlock( prev_ent );
            
            *err = -ENOENT;
            safe_free( fpath );
            
            return NULL;
         }
         
         fskit_entry_unlock( prev_ent );
      }
   } while( true );
   
   safe_free( fpath );
   if( name == NULL ) {
      // ran out of path
      *err = 0;
      
      // check readability
      if( !FSKIT_ENTRY_IS_READABLE( cur_ent->mode, cur_ent->owner, cur_ent->group, user, group ) ) {
         
         fskit_error("User %" PRIu64 " of group %" PRIu64 " cannot read file %" PRIX64 " owned by %" PRIu64 " in group %" PRIu64 "\n",
                user, group, cur_ent->file_id, cur_ent->owner, cur_ent->group );
         
         *err = -EACCES;
         fskit_entry_unlock( cur_ent );
         return NULL;
      }
      
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
