Overview
========

Compiling CAF requires CMake and a recent C++ compiler. To get and compile the
sources on UNIX-like systems, type the following in a terminal:

.. code-block:: bash

   git clone https://github.com/actor-framework/actor-framework
   cd actor-framework
   ./configure
   make -C build
   make -C build install [as root, optional]

Running ``configure`` is not a mandatory step. The script merely automates the
CMake setup and makes setting build options slightly more convenient. On
Windows, use CMake directly to generate an MSVC project file.

Features
--------

* Lightweight, fast and efficient actor implementations
* Network transparent messaging
* Error handling based on Erlang's failure model
* Pattern matching for messages as internal DSL to ease development
* Thread-mapped actors for soft migration of existing applications
* Publish/subscribe group communication

Supported Operating Systems
---------------------------

* Linux
* Windows
* macOS
* FreeBSD

Hello World Example
-------------------

.. literalinclude:: /examples/hello_world.cpp
   :language: C++

