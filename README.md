CAF: C++ Actor Framework
========================

CAF is an open source C++11 actor model implementation featuring
lightweight & fast actor implementations, pattern matching for messages,
network transparent messaging, and more.


On the Web
----------

* __Homepage__: http://www.actor-framework.org
* __Developer Blog__: http://blog.actor-framework.org
* __Doxygen (HTML)__: http://www.actor-framework.org/doc
* __Manual (HTML)__: http://www.actor-framework.org/manual
* __Manual (PDF)__: http://www.actor-framework.org/pdf/manual.pdf
* __Mailing List__: https://groups.google.com/d/forum/actor-framework


Get the Sources
---------------

* git clone https://github.com/actor-framework/actor-framework.git
* cd actor-framework


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


Supported Compilers
-------------------

* GCC >= 4.7
* Clang >= 3.2


Supported Operating Systems
---------------------------

* Linux
* Mac OS X
* *Note for MS Windows*: CAF relies on C++11 features such as variadic templates and unrestricted unions. We will support Visual Studio as soon as Microsoft's compiler implements all required C++11 features. In the meantime, you can use CAF via MinGW.


Scientific Use
--------------

If you use CAF in a scientific context, please use the following citation:

```
@inproceedings{cshw-nassp-13,
  author = {Dominik Charousset and Thomas C. Schmidt and Raphael Hiesgen and Matthias W{\"a}hlisch},
  title = {{Native Actors -- A Scalable Software Platform for Distributed, Heterogeneous Environments}},
  booktitle = {Proc. of the 4rd ACM SIGPLAN Conference on Systems, Programming, and Applications (SPLASH '13), Workshop AGERE!},
  month = {Oct.},
  year = {2013},
  publisher = {ACM},
  address = {New York, NY, USA},
}
```

You will find the paper on http://dx.doi.org/10.1145/2541329.2541336
