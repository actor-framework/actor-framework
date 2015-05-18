# CAF: C++ Actor Framework

[![Jenkins][jenkins-badge]]
(http://mobi39.cpt.haw-hamburg.de/view/CAF%20Dashboard)
[![Gitter][gitter-badge]]
(https://gitter.im/actor-framework/chat)
[jenkins-badge]:
http://mobi39.cpt.haw-hamburg.de/buildStatus/icon?job=CAF/master%20branch
[gitter-badge]:
https://badges.gitter.im/Join%20Chat.svg

CAF is an open source C++11 actor model implementation featuring
lightweight & fast actor implementations, pattern matching for messages,
network transparent messaging, and more.

## On the Web

* __Homepage__: http://www.actor-framework.org
* __Developer Blog__: http://blog.actor-framework.org
* __Doxygen (HTML)__: http://www.actor-framework.org/doc
* __Manual (HTML)__: http://www.actor-framework.org/manual
* __Manual (PDF)__: http://www.actor-framework.org/pdf/manual.pdf
* __Mailing List__: https://groups.google.com/d/forum/actor-framework
* __Chat__: https://gitter.im/actor-framework/chat

## Get the Sources

* git clone https://github.com/actor-framework/actor-framework.git
* cd actor-framework

## Build CAF

The easiest way to build CAF is to use the `configure` script. Other available
options are using [CMake](http://www.cmake.org/) directly or
[SNocs](https://github.com/airutech/snocs).

### Using the `configure` Script

The script is a convenient frontend for `CMake`. See `configure -h`
for a list of available options or read the
[online documentation]
(https://github.com/actor-framework/actor-framework/wiki/Configure-Options).

```
./configure
make
make test
make install [as root, optional]
```

### Using CMake

All available CMake variables are available
[online](https://github.com/actor-framework/actor-framework/wiki/CMake-Options).
CAF also can be included as CMake submodule or added as dependency to other
CMake-based projects using the file `cmake/FindCAF.cmake`.

### Using SNocs

A SNocs workspace is provided by GitHub user
[osblinnikov](https://github.com/osblinnikov) and documented
[online](https://github.com/actor-framework/actor-framework/wiki/Using-SNocs).

## Dependencies

* CMake
* Pthread (until C++11 compilers support the new `thread_local` keyword)

## Supported Compilers

* GCC >= 4.8
* Clang >= 3.2

## Supported Operating Systems

* Linux
* Mac OS X
* FreeBSD 10
* *Note for MS Windows*: CAF relies on C++11 features such as variadic templates
  and unrestricted unions. We will support Visual Studio as soon as Microsoft's
  compiler implements all required C++11 features. In the meantime, you can
  use CAF via MinGW.

## Scientific Use

If you use CAF in a scientific context, please use the following citation:

```
@inproceedings{chs-ccafs-14,
  author = {Dominik Charousset and Raphael Hiesgen and Thomas C. Schmidt},
  title = {{CAF - The C++ Actor Framework for Scalable and Resource-efficient Applications}},
  booktitle = {Proc. of the 5th ACM SIGPLAN Conf. on Systems, Programming, and Applications (SPLASH '14), Workshop AGERE!},
  month = {Oct.},
  year = {2014},
  publisher = {ACM},
  address = {New York, NY, USA},
  location = {Portland, OR},
}
```

You will find the paper online at http://dx.doi.org/10.1145/2687357.2687363
