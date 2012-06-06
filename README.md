libcppa
=======

libcppa is an LGPL C++11 actor model implementation featuring lightweight & fast
actor implementations, pattern matching for messages,
network transparent messaging, and more.


On the Web
----------

* *Blog*: http://libcppa.blogspot.com
* *Manual*: http://neverlord.github.com/libcppa/manual/
* *Documentation*: http://neverlord.github.com/libcppa/


Get the Sources
---------------

* git clone git://github.com/Neverlord/libcppa.git
* cd libcppa


First Steps
-----------

* autoreconf -i
* ./configure
* make
* make install [as root, optional]

It is recommended to run the unit tests as well.

* ./unit_testing/unit_tests

Please submit a bug report that includes (a) your compiler version, (b) your OS,
and (c) the output of the unit tests if an error occurs.


Dependencies
------------

* Automake
* Libtool
* The Boost Library


Supported Compilers
-------------------

* GCC >= 4.7
* Clang >= 3.2


Supported Operating Systems
---------------------------

* Linux
* Mac OS X
