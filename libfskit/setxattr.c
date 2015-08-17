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

#include <fskit/setxattr.h>
#include <fskit/path.h>

#include "fskit_private/private.h"

// set an xattr by path.
// returns whatever fsetxattr returns, plus any errors that can result from path resolution.
int fskit_setxattr( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, char const* name, char const* value, size_t value_len, int flags ) {

   int err = 0;
   int rc = 0;

   // get the fent
   struct fskit_entry* fent = fskit_entry_resolve_path( core, path, user, group, false, &err );
   if( fent == NULL || err != 0 ) {
      return err;
   }

   // set the xattr
   rc = fskit_fsetxattr( core, fent, name, value, value_len, flags );

   fskit_entry_unlock( fent );

   return rc;
}


// set an xattr value on a fskit_entry.
// fent must be at least read-locked
// on success, return 0
// on error:
// * return -EINVAL if both XATTR_CREATE and XATTR_REPLACE are set
// * if XATTR_CREATE is set and the attribute already exists, return EEXIST
// * if XATTR_REPLACE is set and the attribute does not exist, return ENOATTR
// * if there is no memory, return -ENOSPC
// NOTE: fent must be write-locked
int fskit_fsetxattr( struct fskit_core* core, struct fskit_entry* fent, char const* name, char const* value, size_t value_len, int flags ) {

   int rc = 0;
   
   // check for invalid flags
   if( (flags & XATTR_REPLACE) && (flags & XATTR_CREATE) ) {
      return -EINVAL;
   }

   rc = fskit_xattr_set_insert( &fent->xattrs, name, value, value_len, flags );
   if( rc == -ENOMEM ) {
      
      rc = -ENOSPC;
   }
   
   return rc;
}
