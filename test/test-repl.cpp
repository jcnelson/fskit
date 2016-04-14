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

#include "test-repl.h"

#ifdef _FSKIT_REPL

char* test_input = 
"create 0 0 /foo 0644\n"
"access 0 0 /foo 0644\n"
"chmod 0 0 /foo 0644\n"
"chown 0 0 /foo 0644\n"
"mkdir 0 0 /bar 0755\n"
"mknod 0 0 /baz 1 0644\n"
"write 0 abcde 0\n"
"read 0 0\n"
"sync 0\n"
"close 0\n"
"opendir 0 0 /bar\n"
"readdir 0 2\n"
"closedir 0\n"
"setxattr /foo abcde fghij 0\n"
"getxattr /foo abcde\n"
"listxattr /foo\n"
"removexattr /foo abcde\n"
"link 0 0 /foo /foo2\n"
"unlink 0 0 /foo2\n"
"rename 0 0 /foo /foo3\n"
"stat 0 0 /foo3\n"
"symlink 0 0 /foo3 /foo4\n"
"utimes 0 0 /foo4 1 2 3 4\n"
"rmdir 0 0 /bar\n"
"unlink 0 0 /baz\n"
"unlink 0 0 /foo3\n"
"unlink 0 0 /foo4\n"
"statvfs 0 0 /\n";

int main( int argc, char** argv ) {

   struct fskit_core* core = NULL;
   void* output = NULL;
   struct fskit_repl* repl = NULL;
   int rc = 0;

   rc = fskit_test_begin( &core, NULL );
   if( rc != 0 ) {
      exit(1);
   }

   FILE* f = fmemopen( test_input, strlen(test_input), "r");
   if( f == NULL ) {
      perror("fmemopen");
      exit(1);
   }

   repl = fskit_repl_new( core );
   if( repl == NULL ) {
      exit(1);
   }

   rc = fskit_repl_main( repl, f );
   
   fskit_repl_free( repl );
   fskit_test_end( core, &output );
   return 0;
}

#else 

int main( int argc, char** argv ) {
   printf("no repl support compiled (pass REPL=1 to the main Makefile to enable)\n");
   return 0;
}

#endif
