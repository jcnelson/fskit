fskit: Batteries-Included Filesystems
===================================================

fskit is an SDK for creating filesystems.  It provides generic code and data structures for managing filesystem inodes, handles, permissions, reference counting, locking, and so on, so developers can focus on filesystem-specific features.  With fskit, a fully-featured multithreaded filesystem can be written in just over 200 lines of code.

Features
--------
* **True filesystem**.  An fskit-powered filesystem is internally organized into a DAG of programmable inodes, and offers POSIX-style path resolution, ownership, and access controls by default.
* **Multithreaded**.  All fskit-methods are thread-safe and reentrant, and can run concurrently.  fskit lets developers set the concurrency semantics over inodes and sets of paths.
* **Extensible**.  Developers add filesystem-specific behavior by mapping an operation over a set of paths (defined by a regex) to a callback, similar WSGI-style routes.  They can also extend the inodes and file handle data structures with filesystem-specific data and have fskit track it for them.

Building
--------
To build:

    $ make
  
Installing
----------

To install libfskit to /usr/local/lib and headers to /usr/local/include/fskit:

    $ sudo make install
  
To build libfskit_fuse, a helper library that wraps fskit into FUSE bindings:

    $ make -C fuse/
  
To install libfskit_fuse to /usr/local/lib and headers to /usr/local/include/fskit/fuse:

    $ sudo make -C fuse/ install

Documentation
-------------
Forthcoming :)  Take a look at demo/ and tests/ to see examples.
