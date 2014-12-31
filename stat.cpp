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


#include "stat.h"
#include "route.h"

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
   
   return cbrc;
}

// stat a path.
// fill in the stat buffer on success.
// return the usual path resolution errors.
int fskit_stat( struct fskit_core* core, char const* fs_path, uint64_t user, uint64_t group, struct stat* sb ) {
   
   int err = 0;
   int rc = 0;
   
   // get the fent
   struct fskit_entry* fent = fskit_entry_resolve_path( core, fs_path, user, group, false, &err );
   if( fent == NULL || err != 0 ) {
      return err;
   }

   // stat it 
   rc = fskit_fstat( core, fs_path, fent, sb );
   
   fskit_entry_unlock( fent );
   return rc;
}

// stat an inode directly
// fill in the stat buffer
int fskit_fstat( struct fskit_core* core, char const* fs_path, struct fskit_entry* fent, struct stat* sb ) {
   
   int type = 0;
   if( fent->type == FSKIT_ENTRY_TYPE_FILE ) {
      type = S_IFREG;
   }
   else if( fent->type == FSKIT_ENTRY_TYPE_DIR ) {
      type = S_IFDIR;
   }
   else if( fent->type == FSKIT_ENTRY_TYPE_FIFO ) {
      type = S_IFIFO;
   }
   else if( fent->type == FSKIT_ENTRY_TYPE_SOCK ) {
      type = S_IFSOCK;
   }
   else if( fent->type == FSKIT_ENTRY_TYPE_BLK ) {
      type = S_IFBLK;
   }
   else if( fent->type == FSKIT_ENTRY_TYPE_CHR ) {
      type = S_IFCHR;
   }
   else if( fent->type == FSKIT_ENTRY_TYPE_LNK ) {
      type = S_IFLNK;
   }
   
   // fill in defaults
   sb->st_dev = 0;
   sb->st_ino = fent->file_id;
   sb->st_mode = type | fent->mode;
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
   
   // route to user callback 
   int rc = fskit_do_user_stat( core, fs_path, fent, sb );
   
   return rc;
}
