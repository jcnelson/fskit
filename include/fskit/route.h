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

#ifndef _FSKIT_ROUTE_H_
#define _FSKIT_ROUTE_H_

#include <regex.h>

#include <fskit/common.h>
#include <fskit/fskit.h>

// prototypes
struct fskit_core;
struct fskit_dir_entry;
struct fskit_path_route;

// route match methods
#define FSKIT_ROUTE_MATCH_CREATE                0
#define FSKIT_ROUTE_MATCH_MKDIR                 1
#define FSKIT_ROUTE_MATCH_MKNOD                 2
#define FSKIT_ROUTE_MATCH_OPEN                  3
#define FSKIT_ROUTE_MATCH_READDIR               4
#define FSKIT_ROUTE_MATCH_READ                  5
#define FSKIT_ROUTE_MATCH_WRITE                 6
#define FSKIT_ROUTE_MATCH_TRUNC                 7
#define FSKIT_ROUTE_MATCH_CLOSE                 8
#define FSKIT_ROUTE_MATCH_DETACH                9
#define FSKIT_ROUTE_MATCH_STAT                  10
#define FSKIT_ROUTE_MATCH_SYNC                  11
#define FSKIT_ROUTE_MATCH_RENAME                12
#define FSKIT_ROUTE_NUM_ROUTE_TYPES             13

// route consistency disciplines
#define FSKIT_SEQUENTIAL        1       // route method calls will be serialized
#define FSKIT_CONCURRENT        2       // route method calls will be concurrent
#define FSKIT_INODE_SEQUENTIAL  3       // route method calls on the same inode will be serialized
#define FSKIT_INODE_CONCURRENT  4       // route method calls on the same inode will be concurrent, provided that they only read the inode (i.e. the inode will be read-locked)

// common routes
#define FSKIT_ROUTE_ANY         "/([^/]+[/]*)*"


// metadata about the patch matched to the route
struct fskit_route_metadata;

// a path route
struct fskit_path_route;

// dispatch arguments
struct fskit_route_dispatch_args;

// method callback signatures to match on path route
typedef int (*fskit_entry_route_create_callback_t)( struct fskit_core*, struct fskit_route_metadata*, struct fskit_entry*, mode_t, void**, void** );
typedef int (*fskit_entry_route_mknod_callback_t)( struct fskit_core*, struct fskit_route_metadata*, struct fskit_entry*, mode_t, dev_t, void** );
typedef int (*fskit_entry_route_mkdir_callback_t)( struct fskit_core*, struct fskit_route_metadata*, struct fskit_entry*, mode_t, void** );
typedef int (*fskit_entry_route_open_callback_t)( struct fskit_core*, struct fskit_route_metadata*, struct fskit_entry*, int, void** );         // open() and opendir()
typedef int (*fskit_entry_route_close_callback_t)( struct fskit_core*, struct fskit_route_metadata*, struct fskit_entry*, void* );              // close() and closedir()
typedef int (*fskit_entry_route_io_callback_t)( struct fskit_core*, struct fskit_route_metadata*, struct fskit_entry*, char*, size_t, off_t, void* );  // read() and write()
typedef int (*fskit_entry_route_trunc_callback_t)( struct fskit_core*, struct fskit_route_metadata*, struct fskit_entry*, off_t, void* );
typedef int (*fskit_entry_route_sync_callback_t)( struct fskit_core*, struct fskit_route_metadata*, struct fskit_entry* );         // fsync(), fdatasync()
typedef int (*fskit_entry_route_stat_callback_t)( struct fskit_core*, struct fskit_route_metadata*, struct fskit_entry*, struct stat* );
typedef int (*fskit_entry_route_readdir_callback_t)( struct fskit_core*, struct fskit_route_metadata*, struct fskit_entry*, struct fskit_dir_entry**, size_t );
typedef int (*fskit_entry_route_detach_callback_t)( struct fskit_core*, struct fskit_route_metadata*, struct fskit_entry*, void* );             // unlink() and rmdir()
typedef int (*fskit_entry_route_rename_callback_t)( struct fskit_core*, struct fskit_route_metadata*, struct fskit_entry*, char const*, struct fskit_entry* );

// I/O continuation for successful read/write/trunc (i.e. to be called with the route's consistency discipline enforced)
typedef int (*fskit_route_io_continuation)( struct fskit_core*, struct fskit_entry*, off_t, ssize_t );

// define various types of routes
int fskit_route_create( struct fskit_core* core, char const* route_regex, fskit_entry_route_create_callback_t create_cb, int consistency_discipline );
int fskit_route_mknod( struct fskit_core* core, char const* route_regex, fskit_entry_route_mknod_callback_t create_cb, int consistency_discipline );
int fskit_route_mkdir( struct fskit_core* core, char const* route_regex, fskit_entry_route_mkdir_callback_t mkdir_cb, int consistency_discipline );
int fskit_route_open( struct fskit_core* core, char const* route_regex, fskit_entry_route_open_callback_t open_cb, int consistency_discipline );
int fskit_route_close( struct fskit_core* core, char const* route_regex, fskit_entry_route_close_callback_t close_cb, int consistency_discipline );
int fskit_route_readdir( struct fskit_core* core, char const* route_regex, fskit_entry_route_readdir_callback_t readdir_cb, int consistency_discipline );
int fskit_route_read( struct fskit_core* core, char const* route_regex, fskit_entry_route_io_callback_t io_cb, int consistency_discipline );
int fskit_route_write( struct fskit_core* core, char const* route_regex, fskit_entry_route_io_callback_t io_cb, int consistency_discipline );
int fskit_route_trunc( struct fskit_core* core, char const* route_regex, fskit_entry_route_trunc_callback_t io_cb, int consistency_discipline );
int fskit_route_detach( struct fskit_core* core, char const* route_regex, fskit_entry_route_detach_callback_t detach_cb, int consistency_discipline );
int fskit_route_stat( struct fskit_core* core, char const* route_regex, fskit_entry_route_stat_callback_t stat_cb, int consistency_discipline );
int fskit_route_sync( struct fskit_core* core, char const* route_regex, fskit_entry_route_sync_callback_t sync_cb, int consistency_discipline );
int fskit_route_rename( struct fskit_core* core, char const* route_regex, fskit_entry_route_rename_callback_t rename_cb, int consistency_discipline );

// undefine various types of routes
int fskit_unroute_create( struct fskit_core* core, int route_handle );
int fskit_unroute_mknod( struct fskit_core* core, int route_handle );
int fskit_unroute_mkdir( struct fskit_core* core, int route_handle );
int fskit_unroute_open( struct fskit_core* core, int route_handle );
int fskit_unroute_close( struct fskit_core* core, int route_handle );
int fskit_unroute_readdir( struct fskit_core* core, int route_handle );
int fskit_unroute_read( struct fskit_core* core, int route_handle );
int fskit_unroute_write( struct fskit_core* core, int route_handle );
int fskit_unroute_trunc( struct fskit_core* core, int route_handle );
int fskit_unroute_detach( struct fskit_core* core, int route_handle );
int fskit_unroute_stat( struct fskit_core* core, int route_handle );
int fskit_unroute_sync( struct fskit_core* core, int route_handle );
int fskit_unroute_rename( struct fskit_core* core, int route_handle );

// unroute everything 
int fskit_unroute_all( struct fskit_core* core );

// access route metadata 
char* fskit_route_metadata_get_path( struct fskit_route_metadata* route_metadata );
int fskit_route_metadata_num_match_groups( struct fskit_route_metadata* route_metadata );
char** fskit_route_metadata_get_match_groups( struct fskit_route_metadata* route_metadata );
struct fskit_entry* fskit_route_metadata_get_parent( struct fskit_route_metadata* route_metadata );
char* fskit_route_metadata_new_get_path( struct fskit_route_metadata* route_metadata );
struct fskit_entry* fskit_route_metadata_get_new_parent( struct fskit_route_metadata* route_metadata );

#endif
