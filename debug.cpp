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

#include "debug.h"

int _debug_locks = 0;
int _DEBUG_MESSAGES = 1;
int _ERROR_MESSAGES = 1;

int _DEBUG = 1;

void fskit_set_debug_level( int d ) {
   _DEBUG_MESSAGES = d;
}

void fskit_set_error_level( int e ) {
   _ERROR_MESSAGES = e;
}

int fskit_get_debug_level() {
   return _DEBUG_MESSAGES;
}

int fskit_get_error_level() {
   return _ERROR_MESSAGES;
}