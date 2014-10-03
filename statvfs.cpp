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

#include "statvfs.h"
#include "fskit.h"

// stat the filesystem that holds the path.
// the following fields are filled in:
// * f_fsid
// * f_files
// * f_namemax
// the rest are filled with 0's
// return 0 and fill in the statvfs buffer on success.
// return the usual path resolution errors.
int fskit_statvfs( struct fskit_core* core, char const* fs_path, uint64_t user, uint64_t group, struct statvfs* vfs ) {
   
   int err = 0;
   int rc = 0;
   
   // get the fent
   struct fskit_entry* fent = fskit_entry_resolve_path( core, fs_path, user, group, false, &err );
   if( fent == NULL || err != 0 ) {
      return err;
   }

   // stat the filesystem
   rc = fskit_fstatvfs( core, fent, vfs );
   
   fskit_entry_unlock( fent );
   return rc;
}

// stat the filesystem from an inode
// fill in the statvfs buffer (always succeeds)
int fskit_fstatvfs( struct fskit_core* core, struct fskit_entry* fent, struct statvfs* vfs ) {
   
   uint64_t num_files = fskit_file_count_update( core, 0 );
   
   vfs->f_bsize = 0;
   vfs->f_frsize = 0;
   vfs->f_blocks = 0;
   vfs->f_bfree = 0;
   vfs->f_bavail = 0;
   vfs->f_files = num_files;
   vfs->f_ffree = 0;
   vfs->f_favail = 0;
   vfs->f_fsid = FSKIT_FILESYSTEM_TYPE;
   vfs->f_flag = 0;
   vfs->f_namemax = FSKIT_FILESYSTEM_NAMEMAX;
   
   return 0;
}
