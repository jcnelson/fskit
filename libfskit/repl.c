/*
   fskit: a library for creating multi-threaded in-RAM filesystems
   Copyright (C) 2016  Jude Nelson

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

/*
 * This module defines a very primitive REPL that is useful for
 * automated testing.  It is not built unless _FSKIT_REPL
 * is defined.
 */

#ifdef _FSKIT_REPL

#include <fskit/fskit.h>
#include <fskit/util.h>
#include "fskit_private/private.h"

#define FSKIT_REPL_ARGC_MAX 10 
#define FSKIT_REPL_FILE_HANDLE_MAX 1024

// REPL statement
struct fskit_repl_stmt {
   char* cmd;
   char* argv[FSKIT_REPL_ARGC_MAX+1];
   int argc;

   char* linebuf;
};

// REPL state 
struct fskit_repl {

   struct fskit_core* core;
   struct fskit_file_handle* filedes[ FSKIT_REPL_FILE_HANDLE_MAX ];
   struct fskit_dir_handle* dirdes[ FSKIT_REPL_FILE_HANDLE_MAX ];
};


// get REPL statement's command
char const* fskit_repl_stmt_command( struct fskit_repl_stmt* stmt ) {
   return (char const*)stmt->cmd;
}

// get REPL statement args and count
char const** fskit_repl_stmt_args( struct fskit_repl_stmt* stmt, int* argc ) {
   *argc = stmt->argc;
   return (char const**)stmt->argv;
}

// free a REPL statement 
void fskit_repl_stmt_free( struct fskit_repl_stmt* stmt ) {
   fskit_safe_free( stmt->linebuf );
   memset( stmt, 0, sizeof(struct fskit_repl_stmt) );
   fskit_safe_free( stmt );
}


// make a repl 
struct fskit_repl* fskit_repl_new( struct fskit_core* core ) {
   struct fskit_repl* repl = CALLOC_LIST( struct fskit_repl, 1 );
   if( repl == NULL ) {
      return NULL;
   }

   repl->core = core;
   return repl;
}

// free a repl; closing all descriptors as well 
void fskit_repl_free( struct fskit_repl* repl ) {

   int rc = 0;
   for( int i = 0; i < FSKIT_REPL_FILE_HANDLE_MAX; i++ ) {

      if( repl->filedes[i] == NULL ) {
         continue;
      }

      rc = fskit_close( repl->core, repl->filedes[i] );
      if( rc == 0 ) {
         repl->filedes[i] = NULL;
      }
      else {
         fskit_error("close(%d) rc = %d\n", i, rc );
         repl->filedes[i] = NULL;
      }
   }

   for( int i = 0; i < FSKIT_REPL_FILE_HANDLE_MAX; i++ ) {

      if( repl->dirdes[i] == NULL ) {
         continue;
      }

      rc = fskit_closedir( repl->core, repl->dirdes[i] );
      if( rc == 0 ) {
         repl->dirdes[i] = NULL;
      }
      else {
         fskit_error("close(%d) rc = %d\n", i, rc );
         repl->dirdes[i] = NULL;
      }
   }

   fskit_safe_free( repl );

   return;
}
          
 
// parse a REPL statement from a file stream.
// Note that we DO NOT ALLOW SPACES between arguments.
// return the new statement on success, and set *_rc to 0
// return NULL on failure, and set *_rc to:
//  -ENOMEM on OOM
//  -EINVAL on invalid line
struct fskit_repl_stmt* fskit_repl_stmt_parse( FILE* input, int* _rc ) {

   int rc = 0;
   ssize_t nr = 0;
   char* linebuf = NULL;
   size_t len = 0;
   char* tmp = NULL;
   char* arg = NULL;
   int i = 0;
   struct fskit_repl_stmt* stmt = NULL;

   nr = getline( &linebuf, &len, input );
   if( nr < 0 ) {
      *_rc = -errno;
      return NULL;
   }

   linebuf[nr-1] = '\0';

   stmt = CALLOC_LIST( struct fskit_repl_stmt, 1 );
   if( stmt == NULL ) {
      fskit_safe_free( linebuf );
      *_rc = -ENOMEM;
      return NULL;
   }

   stmt->linebuf = linebuf;
   stmt->cmd = strtok_r( linebuf, " \t", &tmp );

   if( stmt->cmd == NULL ) {
      fskit_repl_stmt_free( stmt );
      *_rc = -EINVAL;
      return NULL;
   }

   // each argument... 
   for( i = 0; i < FSKIT_REPL_ARGC_MAX; i++ ) {
      arg = strtok_r( NULL, " \t", &tmp );
      if( arg == NULL ) {
         break;
      }

      stmt->argv[i] = arg;
   }

   if( i == FSKIT_REPL_ARGC_MAX ) {
      // no command has this many arguments
      fskit_repl_stmt_free( stmt );
      *_rc = -EINVAL;
      return NULL;
   }
      
   stmt->argc = i;
   *_rc = 0;

   /*
   printf("%s(", stmt->cmd);
   for( i = 0; i < stmt->argc; i++ ) {
      printf("%s, ", stmt->argv[i]);
   }
   printf(")\n");
   */

   return stmt;
}


// insert a filedes 
// return the index (>= 0) on success
// return -ENFILE if we're out of space
static int fskit_repl_filedes_insert( struct fskit_repl* repl, struct fskit_file_handle* fh ) {

   for( int i = 0; i < FSKIT_REPL_FILE_HANDLE_MAX; i++ ) {
      if( repl->filedes[i] == NULL ) {
         repl->filedes[i] = fh;
         return i;
      }
   }

   return -ENFILE;
}


// insert a dirdes
// return the index (>= 0) on success
// return -ENFILE if we're out of space 
static int fskit_repl_dirdes_insert( struct fskit_repl* repl, struct fskit_dir_handle* dh ) {

   for( int i = 0; i < FSKIT_REPL_FILE_HANDLE_MAX; i++ ) {
      if( repl->dirdes[i] == NULL ) {
         repl->dirdes[i] = dh;
         return i;
      }
   }

   return -ENFILE;
}


// clear a filedes by closing it
// return the result of close on success
// return -EBADF if there is no handle
static int fskit_repl_filedes_close( struct fskit_repl* repl, int fd ) {

   int rc = 0;
   if( fd < 0 || fd >= FSKIT_REPL_FILE_HANDLE_MAX ) {
      return -EBADF;
   }

   if( repl->filedes[fd] == NULL ) {
      return -EBADF;
   }

   rc = fskit_close( repl->core, repl->filedes[fd] );
   if( rc == 0 ) {
      repl->filedes[fd] = NULL;
   }

   return rc;
}


// clear a dirdes by closing it 
// return the result of closedir on success 
// return -EBADF if there was no handle 
static int fskit_repl_dirdes_close( struct fskit_repl* repl, int dfd ) {

   int rc = 0;
   if( dfd < 0 || dfd >= FSKIT_REPL_FILE_HANDLE_MAX ) {
      return -EBADF;
   }

   if( repl->dirdes[dfd] == NULL ) {
      return -EBADF;
   }

   rc = fskit_closedir( repl->core, repl->dirdes[dfd] );
   if( rc == 0 ) {
      repl->dirdes[dfd] = NULL;
   }

   return rc;
}


// look up a file descriptor 
// return it on success
// return NULL if not present 
static struct fskit_file_handle* fskit_repl_filedes_lookup( struct fskit_repl* repl, int fd ) {
   
   if( fd < 0 || fd >= FSKIT_REPL_FILE_HANDLE_MAX ) {
      return NULL;
   }

   return repl->filedes[fd];
}


// look up a directory descriptor 
// return it on success
// return NULL if not present 
static struct fskit_dir_handle* fskit_repl_dirdes_lookup( struct fskit_repl* repl, int dfd ) {

   if( dfd < 0 || dfd >= FSKIT_REPL_FILE_HANDLE_MAX ) {
      return NULL;
   }

   return repl->dirdes[dfd];
}


// parse an unsigned int64 
// return 0 on success and set *ret
// return -EINVAL on failure 
static int fskit_repl_stmt_parse_uint64( char* str, uint64_t* ret ) {

   char* tmp = NULL;
   *ret = (uint64_t)strtoull( str, &tmp, 10 );
   if( *ret == 0 && *tmp != '\0' ) {
      return -EINVAL;
   }

   return 0;
}


// get user and group IDs--these are always the first two arguments
// return 0 on success, and set *user_id and *group_id
// return -EINVAL if this is not possible 
static int fskit_repl_stmt_parse_user_group( struct fskit_repl_stmt* stmt, uint64_t* user, uint64_t* group, int offset ) {

   int rc = 0;

   if( stmt->argc < 2 ) {
      return -EINVAL;
   }

   rc = fskit_repl_stmt_parse_uint64( stmt->argv[offset], user );
   if( rc != 0 ) {
      return rc;
   }

   rc = fskit_repl_stmt_parse_uint64( stmt->argv[offset + 1], group );
   if( rc != 0 ) {
      return rc;
   }

   return 0;
}


// parse mode as an octal string 
// return 0 on success, and set *mode 
// return -EINVAL otherwise 
static int fskit_repl_stmt_parse_mode( char* modearg, mode_t* mode ) {

   char* tmp = NULL;
   *mode = strtoul( modearg, &tmp, 8 );
   if( *mode == 0 && *tmp != '\0' ) {
      return -EINVAL;
   }

   return 0;
}


// dispatch a REPL statement
// return the result of the command
int fskit_repl_stmt_dispatch( struct fskit_repl* repl, struct fskit_repl_stmt* stmt ) {

   int rc = 0;
   uint64_t user_id = 0;
   uint64_t group_id = 0;
   uint64_t new_user_id = 0;
   uint64_t new_group_id = 0;
   uint64_t filedes = 0;
   mode_t mode = 0;
   uint64_t flags = 0;
   uint64_t dev = 0;
   char pathbuf[4097];
   char* path = NULL;
   char* newpath = NULL;
   char* attrname = NULL;
   char* attrvalue = NULL;
   char* buf = NULL;
   ssize_t attrlen = 0;
   uint64_t offset = 0;
   uint64_t len = 0;
   uint64_t size = 0;
   uint64_t num_children = 0;
   struct stat sb;
   struct statvfs svfs;
   uint64_t timetmp = 0;
   struct timeval utimes[2];
   struct fskit_dir_entry** children = NULL;
   struct fskit_file_handle* fh = NULL;
   struct fskit_dir_handle* dh = NULL;
   struct fskit_core* core = repl->core;
   
   // sanity check... 
   if( stmt->cmd == NULL ) {
      return -EINVAL;
   }

   if( strcmp(stmt->cmd, "access") == 0 ) {

      // user group path mode
      if( stmt->argc != 4 ) {
         rc = -EINVAL;
         goto fskit_repl_stmt_dispatch_out;
      }

      rc = fskit_repl_stmt_parse_user_group( stmt, &user_id, &group_id, 0 );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      path = stmt->argv[2];
      rc = fskit_repl_stmt_parse_mode( stmt->argv[3], &mode );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      fskit_debug("access('%s', %" PRIu64 ", %" PRIu64 ", %o)\n", path, user_id, group_id, mode );
      rc = fskit_access( core, path, user_id, group_id, mode ); 
   }
   else if( strcmp(stmt->cmd, "chmod") == 0 ) {

      // user group mode path
      if( stmt->argc != 4 ) {
         rc = -EINVAL;
         goto fskit_repl_stmt_dispatch_out;
      }

      rc = fskit_repl_stmt_parse_user_group( stmt, &user_id, &group_id, 0 );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      path = stmt->argv[3];
      rc = fskit_repl_stmt_parse_mode( stmt->argv[2], &mode );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      fskit_debug("chmod('%s', %" PRIu64 ", %" PRIu64 ", %o)\n", path, user_id, group_id, mode );
      rc = fskit_chmod( core, path, user_id, group_id, mode );
   }
   else if( strcmp(stmt->cmd, "chown") == 0 ) {

      // user group path new_user new_group 
      if( stmt->argc != 5 ) {
         rc = -EINVAL;
         goto fskit_repl_stmt_dispatch_out;
      }
      
      rc = fskit_repl_stmt_parse_user_group( stmt, &user_id, &group_id, 0 );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      path = stmt->argv[2];
      rc = fskit_repl_stmt_parse_user_group( stmt, &new_user_id, &new_group_id, 3 );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      fskit_debug("chown('%s', %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ")\n", path, user_id, group_id, new_user_id, new_group_id );
      rc = fskit_chown( core, path, user_id, group_id, new_user_id, new_group_id );
   }
   else if( strcmp(stmt->cmd, "close") == 0 ) {

      // filedes
      if( stmt->argc != 1 ) {
         rc = -EINVAL;
         goto fskit_repl_stmt_dispatch_out;
      }
     
      rc = fskit_repl_stmt_parse_uint64( stmt->argv[0], &filedes );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      fskit_debug("close(%d)\n", (int)filedes );
      rc = fskit_repl_filedes_close( repl, filedes );
   }
   else if( strcmp(stmt->cmd, "closedir") == 0 ) {

      // dirdes 
      if( stmt->argc != 1 ) {
         rc = -EINVAL;
         goto fskit_repl_stmt_dispatch_out;
      }

      rc = fskit_repl_stmt_parse_uint64( stmt->argv[0], &filedes );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      fskit_debug("closedir(%d)\n", (int)filedes );
      rc = fskit_repl_dirdes_close( repl, filedes );
   }
   else if( strcmp(stmt->cmd, "create") == 0 ) {

      // user group path mode
      if( stmt->argc != 4 ) {
         rc = -EINVAL;
         goto fskit_repl_stmt_dispatch_out;
      }

      rc = fskit_repl_stmt_parse_user_group( stmt, &user_id, &group_id, 0 );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      path = stmt->argv[2];
      rc = fskit_repl_stmt_parse_mode( stmt->argv[3], &mode );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      fskit_debug("create('%s', %" PRIu64 ", %" PRIu64 ", %o)\n", path, user_id, group_id, mode );
      fh = fskit_create( core, path, user_id, group_id, mode, &rc );
      if( fh != NULL ) {
         rc = fskit_repl_filedes_insert( repl, fh );
         if( rc < 0 ) {
            fskit_close( core, fh );
            goto fskit_repl_stmt_dispatch_out;
         }
      }
   }
   else if( strcmp(stmt->cmd, "getxattr") == 0 ) {

      // user group path attr
      if( stmt->argc != 4 ) {
         rc = -EINVAL;
         goto fskit_repl_stmt_dispatch_out;
      }

      rc = fskit_repl_stmt_parse_user_group( stmt, &user_id, &group_id, 0 ) ;
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      path = stmt->argv[2];
      attrname = stmt->argv[3];

      fskit_debug("getxattr('%s', %" PRIu64 ", %" PRIu64 ", '%s')\n", path, user_id, group_id, attrname );
      attrlen = fskit_getxattr( core, path, user_id, group_id, attrname, NULL, 0 ); 
      if( attrlen < 0 ) {
         rc = attrlen;
         goto fskit_repl_stmt_dispatch_out;
      }
      
      attrvalue = CALLOC_LIST( char, attrlen+1 );
      if( attrvalue == NULL ) {
         rc = -ENOMEM;
         goto fskit_repl_stmt_dispatch_out;
      }

      attrlen = fskit_getxattr( core, path, user_id, group_id, attrname, attrvalue, attrlen );
      rc = attrlen;
      if( attrlen < 0 ) {
         fskit_safe_free( attrvalue );
         goto fskit_repl_stmt_dispatch_out;
      }

      printf("%s\n", attrvalue);
      fskit_safe_free( attrvalue );
   }
   else if( strcmp(stmt->cmd, "link") == 0 ) {

      // user group path newpath 
      if( stmt->argc != 4 ) {
         rc = -EINVAL;
         goto fskit_repl_stmt_dispatch_out;
      }

      fskit_debug("link('%s', '%s', %" PRIu64 ", %" PRIu64 "\n", path, newpath, user_id, group_id );
      rc = fskit_repl_stmt_parse_user_group( stmt, &user_id, &group_id, 0 );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      path = stmt->argv[2];
      newpath = stmt->argv[3];

      rc = fskit_link( core, path, newpath, user_id, group_id );
   }
   else if( strcmp(stmt->cmd, "listxattr") == 0 ) {

      // user group path 
      if( stmt->argc != 3 ) {
         rc = -EINVAL;
         goto fskit_repl_stmt_dispatch_out;
      }

      rc = fskit_repl_stmt_parse_user_group( stmt, &user_id, &group_id, 0 );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      path = stmt->argv[2];

      fskit_debug("listxattr('%s', %" PRIu64 ", %" PRIu64 "\n", path, user_id, group_id );
      attrlen = fskit_listxattr( core, path, user_id, group_id, NULL, 0 );
      if( attrlen < 0 ) {
         rc = attrlen;
         goto fskit_repl_stmt_dispatch_out;
      }

      attrvalue = CALLOC_LIST( char, attrlen+1 );
      if( attrvalue == NULL ) {
         rc = -ENOMEM;
         goto fskit_repl_stmt_dispatch_out;
      }

      attrlen = fskit_listxattr( core, path, user_id, group_id, attrvalue, attrlen );
      if( attrlen < 0 ) {
         fskit_safe_free( attrvalue );
         goto fskit_repl_stmt_dispatch_out;
      }

      for( offset = 0; offset < (unsigned)attrlen; ) {

         printf("%s\n", attrvalue + offset );
         offset += strlen(attrvalue) + 1;
      }
      fskit_safe_free( attrvalue );
   }
   else if( strcmp(stmt->cmd, "mkdir") == 0 ) {

      // user group path mode
      if( stmt->argc != 4 ) {
         rc = -EINVAL;
         goto fskit_repl_stmt_dispatch_out;
      }

      rc = fskit_repl_stmt_parse_user_group( stmt, &user_id, &group_id, 0 );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      path = stmt->argv[2];
      rc = fskit_repl_stmt_parse_mode( stmt->argv[3], &mode );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      fskit_debug("mkdir('%s', %" PRIu64 ", %" PRIu64 ", %o)\n", path, user_id, group_id, mode );
      rc = fskit_mkdir( core, path, user_id, group_id, mode );
   }
   else if( strcmp(stmt->cmd, "mknod") == 0 ) {

      // user group path dev mode
      if( stmt->argc != 5 ) {
         rc = -EINVAL;
         goto fskit_repl_stmt_dispatch_out;
      }

      rc = fskit_repl_stmt_parse_user_group( stmt, &user_id, &group_id, 0 );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      path = stmt->argv[2];
      rc = fskit_repl_stmt_parse_mode( stmt->argv[3], &mode );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      rc = fskit_repl_stmt_parse_uint64( stmt->argv[4], &dev );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      fskit_debug("mknod('%s', %" PRIu64 ", %" PRIu64 ", %o)\n", path, user_id, group_id, mode );
      rc = fskit_mknod( core, path, mode, dev, user_id, group_id );
   }
   else if( strcmp(stmt->cmd, "open") == 0 ) {

      // user group path flags 
      if( stmt->argc != 4 ) {
         rc = -EINVAL;
         goto fskit_repl_stmt_dispatch_out;
      }

      rc = fskit_repl_stmt_parse_user_group( stmt, &user_id, &group_id, 0 );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      path = stmt->argv[2];
      rc = fskit_repl_stmt_parse_uint64( stmt->argv[3], &flags );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      fskit_debug("open('%s', %" PRIu64 ", %" PRIu64 ", %" PRIx64 ")\n", path, user_id, group_id, flags );
      fh = fskit_open( core, path, user_id, group_id, 0, mode, &rc );
      if( fh != NULL ) {
         rc = fskit_repl_filedes_insert( repl, fh );
         if( rc < 0 ) {
            fskit_close( core, fh );
            goto fskit_repl_stmt_dispatch_out;
         }
      }
   }
   else if( strcmp(stmt->cmd, "opendir") == 0 ) {

      // user group path 
      if( stmt->argc != 3 ) {
         rc = -EINVAL;
         goto fskit_repl_stmt_dispatch_out;
      }

      rc = fskit_repl_stmt_parse_user_group( stmt, &user_id, &group_id, 0 );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      path = stmt->argv[2];

      fskit_debug("opendir('%s', %" PRIu64 ", %" PRIu64 ")\n", path, user_id, group_id );
      dh = fskit_opendir( core, path, user_id, group_id, &rc );
      if( dh != NULL ) {
         rc = fskit_repl_dirdes_insert( repl, dh );
         if( rc < 0 ) {
            fskit_closedir( core, dh );
            goto fskit_repl_stmt_dispatch_out;
         }
      }

      rc = 0;
   }
   else if( strcmp(stmt->cmd, "read") == 0 ) {

      // filedes offset len 
      if( stmt->argc != 3 ) {
         rc = -EINVAL;
         goto fskit_repl_stmt_dispatch_out;
      }

      rc = fskit_repl_stmt_parse_uint64( stmt->argv[0], &filedes );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      rc = fskit_repl_stmt_parse_uint64( stmt->argv[1], &offset );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      rc = fskit_repl_stmt_parse_uint64( stmt->argv[2], &len );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      fh = fskit_repl_filedes_lookup( repl, filedes );
      if( fh == NULL ) {
         rc = -EBADF;
         goto fskit_repl_stmt_dispatch_out;
      }

      buf = CALLOC_LIST( char, len+1 );
      if( buf == NULL ) {
         rc = -ENOMEM;
         goto fskit_repl_stmt_dispatch_out;
      }

      fskit_debug("read(%" PRIu64 ", %" PRIu64 ", %" PRIu64 ")\n", filedes, offset, len );
      rc = fskit_read( core, fh, buf, len, offset );
      if( rc < 0 ) {
         fskit_safe_free( buf );
         goto fskit_repl_stmt_dispatch_out;
      }

      printf("%s", buf);
      fskit_safe_free( buf );
      rc = 0;
   }
   else if( strcmp(stmt->cmd, "readdir") == 0 ) {

      // dirdes num_children 
      if( stmt->argc != 3 ) {
         rc = -EINVAL;
         goto fskit_repl_stmt_dispatch_out;
      }

      rc = fskit_repl_stmt_parse_uint64( stmt->argv[0], &filedes );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      rc = fskit_repl_stmt_parse_uint64( stmt->argv[1], &num_children );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      dh = fskit_repl_dirdes_lookup( repl, filedes );
      if( dh == NULL ) {
         rc = -EBADF;
         goto fskit_repl_stmt_dispatch_out;
      }

      fskit_debug("readdir(%" PRIu64 ", %" PRIu64 ")\n", filedes, num_children);
      children = fskit_readdir( core, dh, num_children, &len, &rc );
      if( children == NULL ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      for( uint64_t i = 0; i < len; i++ ) {

         struct fskit_dir_entry* dent = children[i];
         printf("%d %" PRIX64 " '%s'\n", dent->type, dent->file_id, dent->name );
      }

      fskit_dir_entry_free_list( children );
      rc = 0;
   }
   else if( strcmp(stmt->cmd, "readlink") == 0 ) {

      // user group path 
      if( stmt->argc != 3 ) {
         rc = -EINVAL;
         goto fskit_repl_stmt_dispatch_out;
      }
      
      rc = fskit_repl_stmt_parse_user_group( stmt, &user_id, &group_id, 0 );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      path = stmt->argv[2];
      memset( pathbuf, 0, 4097 );

      fskit_debug("readlink('%s', %" PRIu64 ", %" PRIu64 ")\n", path, user_id, group_id);
      rc = fskit_readlink( core, path, user_id, group_id, pathbuf, 4096 );
      if( rc < 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }
      
      printf("%s\n", pathbuf);
      memset( pathbuf, 0, 4097);
   }
   else if( strcmp(stmt->cmd, "removexattr") == 0 ) {

      // user group path attrname 
      if( stmt->argc != 4 ) {
         rc = -EINVAL;
         goto fskit_repl_stmt_dispatch_out;
      }

      rc = fskit_repl_stmt_parse_user_group( stmt, &user_id, &group_id, 0 );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      path = stmt->argv[2];
      attrname = stmt->argv[3];

      fskit_debug("removexattr('%s', %" PRIu64 ", %" PRIu64 ", '%s')\n", path, user_id, group_id, attrname );
      rc = fskit_removexattr( core, path, user_id, group_id, attrname );
      if( rc < 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }
   }
   else if( strcmp(stmt->cmd, "rename") == 0 ) {

      // user group path newpath 
      if( stmt->argc != 4 ) {
         rc = -EINVAL;
         goto fskit_repl_stmt_dispatch_out;
      }

      rc = fskit_repl_stmt_parse_user_group( stmt, &user_id, &group_id, 0 );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      path = stmt->argv[2];
      newpath = stmt->argv[3];

      fskit_debug("rename('%s', %" PRIu64 ", %" PRIu64 ", '%s')\n", path, user_id, group_id, newpath );
      rc = fskit_rename( core, path, newpath, user_id, group_id );
      if( rc < 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }
   }
   else if( strcmp(stmt->cmd, "rmdir") == 0 ) {

      // user group path 
      if( stmt->argc != 3 ) {
         rc = -EINVAL;
         goto fskit_repl_stmt_dispatch_out;
      }

      rc = fskit_repl_stmt_parse_user_group( stmt, &user_id, &group_id, 0 );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      path = stmt->argv[2];

      fskit_debug("rmdir('%s', %" PRIu64 ", %" PRIu64 ")\n", path, user_id, group_id );
      rc = fskit_rmdir( core, path, user_id, group_id );
      if( rc < 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }
   }
   else if( strcmp(stmt->cmd, "setxattr") == 0 ) {

      // user group path attrname attrvalue flags 
      if( stmt->argc != 6 ) {
         rc = -EINVAL;
         goto fskit_repl_stmt_dispatch_out;
      }

      rc = fskit_repl_stmt_parse_user_group( stmt, &user_id, &group_id, 0 );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      path = stmt->argv[2];
      attrname = stmt->argv[3];
      attrvalue = stmt->argv[4];

      rc = fskit_repl_stmt_parse_uint64( stmt->argv[5], &flags );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      fskit_debug("setxattr('%s', %" PRIu64 ", %" PRIu64 ", '%s', '%s', %" PRIx64 ")\n", path, user_id, group_id, attrname, attrvalue, flags );
      rc = fskit_setxattr( core, path, user_id, group_id, attrname, attrvalue, strlen(attrvalue), flags );
      if( rc < 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }
   }
   else if( strcmp(stmt->cmd, "stat") == 0 ) {

      // user group path 
      if( stmt->argc != 3 ) {
         rc = -EINVAL;
         goto fskit_repl_stmt_dispatch_out;
      }

      rc = fskit_repl_stmt_parse_user_group( stmt, &user_id, &group_id, 0 );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      path = stmt->argv[2];

      fskit_debug("stat('%s', %" PRIu64 ", %" PRIu64 ")\n", path, user_id, group_id );
      rc = fskit_stat( core, path, user_id, group_id, &sb );
      if( rc < 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      printf("st_dev=%" PRIu64 ", st_ino=%" PRIX64 ", st_mode=%o, st_nlink=%" PRIu64 ", st_uid=%" PRIu64 ", st_gid=%" PRIu64 ", st_rdev=%" PRIu64 \
             ", st_size=%" PRIu64 ", st_blksize=%" PRIu64 ", st_blocks=%" PRIu64 ", st_atime=%" PRIu64 ".%ld\n, st_mtime=%" PRIu64 ".%ld, st_ctime=%" PRIu64 ".%ld\n",
             sb.st_dev, sb.st_ino, sb.st_mode, sb.st_nlink, (uint64_t)sb.st_uid, (uint64_t)sb.st_gid, sb.st_rdev, sb.st_size, sb.st_blksize, sb.st_blocks,
             sb.st_atim.tv_sec, sb.st_atim.tv_nsec, sb.st_mtim.tv_sec, sb.st_mtim.tv_nsec, sb.st_ctim.tv_sec, sb.st_ctim.tv_nsec );
             
   }
   else if( strcmp(stmt->cmd, "statvfs") == 0 ) {

      // user group path 
      if( stmt->argc != 3 ) {
         rc = -EINVAL;
         goto fskit_repl_stmt_dispatch_out;
      }

      rc = fskit_repl_stmt_parse_user_group( stmt, &user_id, &group_id, 0 );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      path = stmt->argv[2];

      fskit_debug("statvfs('%s', %" PRIu64 ", %" PRIu64 ")\n", path, user_id, group_id );
      rc = fskit_statvfs( core, path, user_id, group_id, &svfs );
      if( rc < 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      printf("f_bsize=%lu, f_frsize=%lu, f_blocks=%" PRIu64 ", f_bfree=%" PRIu64 ", f_bavail=%" PRIu64 ", f_files=%" PRIu64 ", f_ffree=%" PRIu64 ", f_favail=%" PRIu64 ", f_fsid=%lu, f_flag=%lu, f_namemax=%lu\n",
            svfs.f_bsize, svfs.f_frsize, svfs.f_blocks, svfs.f_bfree, svfs.f_bavail, svfs.f_files, svfs.f_ffree, svfs.f_favail, svfs.f_fsid, svfs.f_flag, svfs.f_namemax );
   }
   else if( strcmp(stmt->cmd, "symlink") == 0 ) {

      // user group path newpath 
      if( stmt->argc != 4 ) {
         rc = -EINVAL;
         goto fskit_repl_stmt_dispatch_out;
      }

      rc = fskit_repl_stmt_parse_user_group( stmt, &user_id, &group_id, 0 );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      path = stmt->argv[2];
      newpath = stmt->argv[3];

      fskit_debug("symlink('%s', %" PRIu64 ", %" PRIu64 ", '%s')\n", path, user_id, group_id, newpath );
      rc = fskit_symlink( core, path, newpath, user_id, group_id );
      if( rc < 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }
   }
   else if( strcmp(stmt->cmd, "sync") == 0 ) {

      // filedes 
      if( stmt->argc != 1 ) {
         rc = -EINVAL;
         goto fskit_repl_stmt_dispatch_out;
      }

      rc = fskit_repl_stmt_parse_uint64( stmt->argv[0], &filedes );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      fh = fskit_repl_filedes_lookup( repl, filedes );
      if( fh == NULL ) {
         rc = -EBADF;
         goto fskit_repl_stmt_dispatch_out;
      }

      fskit_debug("fsync(%" PRIu64 ")\n", filedes );
      rc = fskit_fsync( core, fh );
      if( rc < 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }
   }
   else if( strcmp(stmt->cmd, "trunc") == 0 ) {

      // user group path newsize 
      if( stmt->argc != 4 ) {
         rc = -EINVAL;
         goto fskit_repl_stmt_dispatch_out;
      }

      rc = fskit_repl_stmt_parse_user_group( stmt, &user_id, &group_id, 0 );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      path = stmt->argv[2];
      rc = fskit_repl_stmt_parse_uint64( stmt->argv[3], &size );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      fskit_debug("trunc('%s', %" PRIu64 ")\n", path, size );
      rc = fskit_trunc( core, path, user_id, group_id, size );
      if( rc < 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }
   }
   else if( strcmp(stmt->cmd, "unlink") == 0 ) {

      // user group path 
      if( stmt->argc != 3 ) {
         rc = -EINVAL;
         goto fskit_repl_stmt_dispatch_out;
      }

      rc = fskit_repl_stmt_parse_user_group( stmt, &user_id, &group_id, 0 );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      path = stmt->argv[2];
      
      fskit_debug("unlink('%s', %" PRIu64 ", %" PRIu64 ")\n", path, user_id, group_id );
      rc = fskit_unlink( core, path, user_id, group_id );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }
   }
   else if( strcmp(stmt->cmd, "utime") == 0 ) {

      // user group path atime_sec atime_usec mtime_sec mtime_usec
      if( stmt->argc != 7 ) {
         rc = -EINVAL;
         goto fskit_repl_stmt_dispatch_out;
      }

      rc = fskit_repl_stmt_parse_user_group( stmt, &user_id, &group_id, 0 );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      memset( utimes, 0, sizeof(struct timeval) * 2);

      path = stmt->argv[2];
      rc = fskit_repl_stmt_parse_uint64( stmt->argv[3], &timetmp );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      utimes[0].tv_sec = timetmp;

      rc = fskit_repl_stmt_parse_uint64( stmt->argv[4], &timetmp );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      utimes[0].tv_usec = timetmp;

      rc = fskit_repl_stmt_parse_uint64( stmt->argv[5], &timetmp );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      utimes[1].tv_sec = timetmp;

      rc = fskit_repl_stmt_parse_uint64( stmt->argv[6], &timetmp );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      utimes[1].tv_usec = timetmp;

      fskit_debug("fskit_utimes('%s', %" PRIu64 ", %" PRIu64 ", [(%" PRId64 ", %ld), (%" PRId64 ", %ld)])\n",
            path, user_id, group_id, utimes[0].tv_sec, utimes[0].tv_usec, utimes[1].tv_sec, utimes[1].tv_usec );

      rc = fskit_utimes( core, path, user_id, group_id, utimes );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }
   }
   else if( strcmp(stmt->cmd, "write") == 0 ) {

      // filedes string offset 
      if( stmt->argc != 3 ) {
         rc = -EINVAL;
         goto fskit_repl_stmt_dispatch_out;
      }

      rc = fskit_repl_stmt_parse_uint64( stmt->argv[0], &filedes );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      buf = stmt->argv[1];

      rc = fskit_repl_stmt_parse_uint64( stmt->argv[2], &offset );
      if( rc != 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }

      fh = fskit_repl_filedes_lookup( repl, filedes );
      if( fh == NULL ) {
         rc = -EBADF;
         goto fskit_repl_stmt_dispatch_out;
      }

      fskit_debug("fskit_write(%" PRIu64 ", '%s', %" PRIu64 ")\n", filedes, buf, offset );
      rc = fskit_write( core, fh, buf, strlen(buf), offset );
      if( rc < 0 ) {
         goto fskit_repl_stmt_dispatch_out;
      }
   }
   else {

      fskit_error("Unrecognized command '%s'\n", stmt->cmd );
      rc = -EINVAL;
   } 
         
fskit_repl_stmt_dispatch_out:

   return rc;
}


// main REPL loop
// reads commands from the given file until EOF,
// and dispatches them to the given repl.
// returns the status of the last line processed
int fskit_repl_main( struct fskit_repl* repl, FILE* f ) {

   int rc = 0;
   struct fskit_repl_stmt* stmt = NULL;

   while( !feof( f ) ) {

       stmt = fskit_repl_stmt_parse( f, &rc );
       if( stmt == NULL ) {
          fskit_error("%s", "fskit_repl_stmt_parse failed\n");
          continue;
       }

       rc = fskit_repl_stmt_dispatch( repl, stmt );
       if( rc != 0 ) {
          fskit_error("fskit_repl_stmt_dispatch('%s') rc = %d\n", stmt->cmd, rc );
       }
       
       fskit_repl_stmt_free( stmt );
   }

   return rc;
}

#endif
