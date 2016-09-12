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

#ifndef _FSKIT_REPL_H_
#define _FSKIT_REPL_H_

#include <fskit/debug.h>
#include <fskit/entry.h>

#include <attr/xattr.h>

FSKIT_C_LINKAGE_BEGIN

struct fskit_repl;
struct fskit_repl_stmt;

struct fskit_repl* fskit_repl_new( struct fskit_core* core );
void fskit_repl_free( struct fskit_repl* repl );

char const* fskit_repl_stmt_command( struct fskit_repl_stmt* stmt );
char const** fskit_repl_stmt_args( struct fskit_repl_stmt*, int* argc );

void fskit_repl_stmt_free( struct fskit_repl_stmt* stmt );

struct fskit_repl_stmt* fskit_repl_stmt_parse( FILE* input, int* rc );

int fskit_repl_stmt_dispatch( struct fskit_repl* repl, struct fskit_repl_stmt* stmt );

int fskit_repl_main( struct fskit_repl* repl, FILE* f );

FSKIT_C_LINKAGE_END

#endif
