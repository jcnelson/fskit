fskit: Batteries-Included Filesystems
===================================================

fskit is an SDK for creating filesystems.  It provides generic code and data structures for managing filesystem inodes, handles, permissions, reference counting, concurrency, and so on, so developers can focus on filesystem-specific features.  With fskit, a fully-featured multithreaded filesystem can be written in just over 200 lines of code.

Why fskit?
---------
When implementing a userspace filesystem, existing filesystem libraries ([FUSE](http://fuse.sourceforge.net/), [puffs](http://www.netbsd.org/docs/puffs/), [9P](http://9p.cat-v.org/documentation/)) force the programmer to think about both read/write handling ("data plane" operations) and metadata bookkeeping and access control ("control plane" operations).  This makes implementation difficult in two key ways:
* Most of a filesystem's complexity is in its control plane, since it includes all filesystem code and state except for those needed by read(), write(), and trunc().
* Most of the control plane code and metadata are common to all filesystems, meaning that implementing a new filesystem with existing libraries usually duplicates a lot of effort.

To eliminate these difficulties, we created fskit.  Fskit implements a featureful but extensible POSIX-like filesystem control plane on top of an existing filesystem library, freeing the programmer to focus on the much simpler application-specific data plane.  In doing so, we lower the barrier-to-entry for creating new filesystems.

Design Goals
-------------
* **Stable, extensible thread-safe API.**  Developers declare WSGI-style routes to I/O handlers and set the consistency discipline to be enforced when running them.  The data plane is fully defined by the developer.  We offer an extensible control-plane with POSIX filesystem semantics by default (balancing programmer freedom with the principle of least surprise).
* **Minimal runtime dependencies.**  Fskit should be amenable to static linking, if need be.
* **Multi-threaded architecture.**  Fskit supports concurrent I/O by default.
* **Portable architecture.**  Fskit should support all POSIX-y OSs that offer a way to run filesystems in userspace.
* **C linkage.**  Despite being written in C++, fskit binaries will provide C linkage with the documented API.

Non-Goals
---------
* We're not going to re-implement the kernel/userspace filesystem layer.  Instead we'll leverage existing mechanisms, like FUSE, 9P, or kdbus.
* We're not going to tie ourselves to any particular OS or filesystem library--we will make every reasonable effort to accept patches that enhance portability.
* We're not going to handle data persistence, caching, or holding onto application data.  It's the application's job to manage its data; it's fskit's job to organize it into a filesystem.
* We're not going to keep metadata persistent across mounts.  However, we will help the application serialize it if it wants to do so itself.
* We're not going to worry about network transparency.  There are just too many domain-specific aspects to doing this correctly for every application (examples: partition tolerance versus consistency versus availability trade-offs, write-conflict resolution, at-most-once versus at-least-once semantics, security and key distribution, etc.).  However, if the underlying IPC mechanism can handle it (e.g. 9P), we won't get in its way, nor will we get in the way of an application's approach to handling this.

Dependencies
------------
* libc
* libpthread
* libstdc++
* librt
* libfuse (patches alternative backends like 9P and puffs are welcome).

Building
--------
The build process is intentionally simple.  To build:

    $ make

Installing
----------

To install libfskit to /usr/local/lib and headers to /usr/local/include/fskit:

    $ sudo make install

To build libfskit_fuse, a helper library that wraps fskit into FUSE bindings:

    $ make -C fuse/

To install libfskit_fuse to /usr/local/lib and headers to /usr/local/include/fskit/fuse:

    $ sudo make -C fuse/ install

You can change the installation directory by setting DESTDIR.

Documentation
-------------
Forthcoming :)  Take a look at demo/ and tests/ to see examples.
