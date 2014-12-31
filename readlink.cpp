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

#include "readlink.h"
#include "path.h"

// read the target in a symlink (including the null character at the end)
// return the number of bytes read on success (up to MIN(buflen, linkpath size))
// return -EINVAL if the entry isn't a symlink
// return -EIO if the entry is corrupt (indicates a bug in fskit)
// return -errno if path resolution fails
ssize_t fskit_readlink( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, char* buf, size_t buflen ) {

   int err = 0;
   ssize_t num_read = 0;
   struct fskit_entry* fent = NULL;

   // find this
   fent = fskit_entry_resolve_path( core, path, user, group, false, &err );
   if( err != 0 || fent == NULL ) {
      return err;
   }

   // symlink?
   if( fent->type != FSKIT_ENTRY_TYPE_LNK ) {
      fskit_entry_unlock( fent );
      return -EINVAL;
   }

   // sanity check
   if( fent->symlink_target == NULL ) {

      fskit_error("BUG: fskit entry %" PRIX64 " (at %p) is a symlink, but has no target path set\n", fent->file_id, fent );
      fskit_entry_unlock( fent );
      return -EIO;
   }

   // read it (including null character)
   num_read = (ssize_t)MIN( buflen, (unsigned)fent->size + 1 );

   memcpy( buf, fent->symlink_target, num_read );

   fskit_entry_unlock( fent );
   return num_read;
}
