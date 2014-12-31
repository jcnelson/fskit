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


#include "access.h"
#include "entry.h"
#include "path.h"
#include "stat.h"

int fskit_access( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, mode_t mode ) {
   
   int err = 0;
   struct fskit_entry* fent = fskit_entry_resolve_path( core, path, user, group, false, &err );
   if( !fent || err ) {
      if( !err ) {
         err = -ENOMEM;
      }
      return err;
   }

   // F_OK implicitly satisfied
   // give the application a chance to process the stat buffer
   struct stat sb;
   
   err = fskit_fstat( core, path, fent, &sb );
   if( err == 0 ) {
      
      // check against stat buffer
      if( (mode & R_OK) && !FSKIT_ENTRY_IS_READABLE( sb.st_mode, sb.st_uid, sb.st_gid, user, group ) ) {
         err = -EACCES;
      }
      else if( (mode & W_OK) && !FSKIT_ENTRY_IS_WRITEABLE( sb.st_mode, sb.st_uid, sb.st_gid, user, group ) ) {
         err = -EACCES;
      }
      else if( (mode & X_OK) && !FSKIT_ENTRY_IS_EXECUTABLE( sb.st_mode, sb.st_uid, sb.st_gid, user, group ) ) {
         err = -EACCES;
      }
   }

   fskit_entry_unlock( fent );
   return err;
}