Overview
========

Compiling CAF requires CMake and a C++11-compatible compiler. To get and
compile the sources on UNIX-like systems, type the following in a terminal:

.. code-block:: none

   git clone https://github.com/actor-framework/actor-framework
   cd actor-framework
   ./configure
   make -C build
   make -C build test [optional]
   make -C build install [as root, optional]

If the output of ``make test`` indicates an error, please submit a bug report
that includes (a) your compiler version, (b) your OS, and (c) the content of the
file ``build/Testing/Temporary/LastTest.log``.

The configure script provides several build options for advanced configuration.
Please run ``./configure -h`` to see all available options. If you are building
CAF only as a dependency, disabling the unit tests and the examples can safe you
some time during the build.

.. note::

  The ``configure`` script provides a convenient way for creating a build
  directory and calling CMake. Users that are familiar with CMake can of course
  also use CMake directly and avoid using the ``configure`` script entirely.

  On Windows, we recomment using the CMake GUI to generate a Visual Studio
  project file for CAF.

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

