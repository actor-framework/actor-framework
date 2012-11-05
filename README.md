libcppa
=======

libcppa is an LGPL C++11 actor model implementation featuring lightweight & fast
actor implementations, pattern matching for messages,
network transparent messaging, and more.


On the Web
----------

* __Blog__: http://libcppa.org
* __Manual (PDF)__: http://neverlord.github.com/libcppa/manual/manual.pdf
* __Manual (HTML)__: http://neverlord.github.com/libcppa/manual/index.html
* __Documentation__: http://neverlord.github.com/libcppa/
* __Project Homepage__: http://www.realmv6.org/libcppa.html
* __Mailing List__: https://groups.google.com/d/forum/libcppa

Get the Sources
---------------

* git clone git://github.com/Neverlord/libcppa.git
* cd libcppa


First Steps
-----------

* ./configure
* make
* make install [as root, optional]

It is recommended to run the unit tests as well.

* make test

Please submit a bug report that includes (a) your compiler version, (b) your OS,
and (c) the content of the file build/Testing/Temporary/LastTest.log
if an error occurs.


Dependencies
------------

* CMake
* Pthread (until C++11 compilers support the new `thread_local` keyword)
* *Optional:* Boost.Context (enables context-switching actors)


Supported Compilers
-------------------

* GCC >= 4.7
* Clang >= 3.2


Supported Operating Systems
---------------------------

* Linux
* Mac OS X
* *Note for MS Windows*: libcppa relies on C++11 features such as variadic templates. We will support this platform as soon as Microsoft's compiler implements all required C++11 features.
