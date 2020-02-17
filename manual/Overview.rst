Overview
========

Compiling CAF requires CMake and a C++11-compatible compiler. To get and
compile the sources on UNIX-like systems, type the following in a terminal:

.. ::

   git clone https://github.com/actor-framework/actor-framework
   cd actor-framework
   ./configure
   make
   make install [as root, optional]

We recommended to run the unit tests as well:

.. ::

   make test

If the output indicates an error, please submit a bug report that includes (a)
your compiler version, (b) your OS, and (c) the content of the file
``build/Testing/Temporary/LastTest.log``.

Features
--------

* Lightweight, fast and efficient actor implementations
* Network transparent messaging
* Error handling based on Erlang's failure model
* Pattern matching for messages as internal DSL to ease development
* Thread-mapped actors for soft migration of existing applications
* Publish/subscribe group communication

Minimal Compiler Versions
-------------------------

* GCC 4.8
* Clang 3.4
* Visual Studio 2015, Update 3

Supported Operating Systems
---------------------------

* Linux
* Mac OS X
* Windows (static library only)

Hello World Example
-------------------

.. literalinclude:: /examples/hello_world.cpp
   :language: C++

