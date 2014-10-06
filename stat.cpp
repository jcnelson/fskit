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

#include "stat.h"

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
   rc = fskit_fstat( core, fent, sb );
   
   fskit_entry_unlock( fent );
   return rc;
}

// stat an inode directly
// fill in the stat buffer
int fskit_fstat( struct fskit_core* core, struct fskit_entry* fent, struct stat* sb ) {
   
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
   
   return 0;
}
