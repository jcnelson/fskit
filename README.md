fskit: FUSE with POSIX Batteries Included
=========================================

fskit is a library for creating extensible, thread-safe filesystems using WSGI-style routes.  fskit handles filesystem metadata, concurrency, and access control, so you can focus on writing the filesystem's data plane.

To build libfskit:
  
    $ make
  
To install libfskit to /usr/local/lib and headers to /usr/local/include/fskit:

    $ sudo make install
  
To build libfskit_fuse, a helper library that wraps fskit into FUSE bindings:

    $ make -C fuse/
  
To install libfskit_fuse to /usr/local/lib and headers to /usr/local/include/fskit/fuse:

    $ sudo make -C fuse/ install

Documentation forthcoming.  Take a look at demos/ to see an example.
