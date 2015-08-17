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

#include <fskit/stat.h>
#include <fskit/route.h>

#include "fskit_private/private.h"

int fskit_do_user_stat( struct fskit_core* core, char const* fs_path, struct fskit_entry* fent, struct stat* sb ) {

   int rc = 0;
   int cbrc = 0;

   struct fskit_route_dispatch_args dargs;

   fskit_route_stat_args( &dargs, sb );

   rc = fskit_route_call_stat( core, fs_path, fent, &dargs, &cbrc );

   if( rc == -EPERM || rc == -ENOSYS ) {
      // no stat defined
      return 0;
   }
   else {
       
      fskit_error("fskit_route_call_stat rc = %d, cbrc = %d\n", rc, cbrc );
   }

   return cbrc;
}

// stat a path.
// fill in the stat buffer on success.
// return the usual path resolution errors.
int fskit_stat( struct fskit_core* core, char const* fs_path, uint64_t user, uint64_t group, struct stat* sb ) {

   int rc = 0;

   // ref this entry, so it won't disappear on stat
   struct fskit_entry* fent = fskit_entry_ref( core, fs_path, &rc );
   if( fent == NULL ) {
      
      return rc;
   }
   
   // stat it
   rc = fskit_fstat( core, fs_path, fent, sb );

   fskit_entry_unref( core, fs_path, fent );
   
   return rc;
}

// generate a full mode from the entry's type and permission bits 
mode_t fskit_fullmode( int fskit_type, mode_t mode ) {
   
   int type = 0;
   
   if( fskit_type == FSKIT_ENTRY_TYPE_FILE ) {
      type = S_IFREG;
   }
   else if( fskit_type == FSKIT_ENTRY_TYPE_DIR ) {
      type = S_IFDIR;
   }
   else if( fskit_type == FSKIT_ENTRY_TYPE_FIFO ) {
      type = S_IFIFO;
   }
   else if( fskit_type == FSKIT_ENTRY_TYPE_SOCK ) {
      type = S_IFSOCK;
   }
   else if( fskit_type == FSKIT_ENTRY_TYPE_BLK ) {
      type = S_IFBLK;
   }
   else if( fskit_type == FSKIT_ENTRY_TYPE_CHR ) {
      type = S_IFCHR;
   }
   else if( fskit_type == FSKIT_ENTRY_TYPE_LNK ) {
      type = S_IFLNK;
   }
   else {
      return -EINVAL;
   }
   
   return type | mode;
}



// stat an inode directly
// fill in the stat buffer, but do NOT call the user route
// always succeeds
// NOTE: fent must be read-locked
int fskit_entry_fstat( struct fskit_entry* fent, struct stat* sb ) {
   
   // fill in defaults
   sb->st_dev = 0;
   sb->st_ino = fent->file_id;
   sb->st_mode = fskit_fullmode( fent->type, fent->mode );
   sb->st_nlink = fent->link_count;
   sb->st_uid = fent->owner;
   sb->st_gid = fent->group;
   sb->st_rdev = fent->dev;
   sb->st_size = fent->size;
   sb->st_blksize = 0;
   sb->st_blocks = 0;

   sb->st_atim.tv_sec = fent->atime_sec;
   sb->st_atim.tv_nsec = fent->atime_nsec;

   sb->st_mtim.tv_sec = fent->mtime_sec;
   sb->st_mtim.tv_nsec = fent->mtime_nsec;

   sb->st_ctim.tv_sec = fent->ctime_sec;
   sb->st_ctim.tv_nsec = fent->ctime_nsec;

   return 0;
}


// stat an inode directly
// fill in the stat buffer, and call the user route
// NOTE: fent must be read-locked
// returns the result of the user route
int fskit_fstat( struct fskit_core* core, char const* fs_path, struct fskit_entry* fent, struct stat* sb ) {
   
   // fill in defaults
   fskit_entry_rlock( fent );
   fskit_entry_fstat( fent, sb );
   fskit_entry_unlock( fent );
   
   // route to user callback
   int rc = fskit_do_user_stat( core, fs_path, fent, sb );

   return rc;
}
