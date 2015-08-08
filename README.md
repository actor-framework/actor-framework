# CAF: C++ Actor Framework

CAF is an open source C++11 actor model implementation featuring
lightweight & fast actor implementations, pattern matching for messages,
network transparent messaging, and more.

[![Gitter][gitter-badge]]
(https://gitter.im/actor-framework/chat)
[![Jenkins][jenkins-badge]]
(http://mobi39.cpt.haw-hamburg.de/view/CAF%20Dashboard)
[![Coverity][coverity-badge]]
(https://scan.coverity.com/projects/5555)
[![In Progress][in-progress-badge]]
(https://waffle.io/actor-framework/actor-framework)
[![Fixed in Develop][fixed-in-develop-badge]]
(https://waffle.io/actor-framework/actor-framework)

## On the Web

* __Chat__: https://gitter.im/actor-framework/chat
* __Homepage__: http://www.actor-framework.org
* __Developer Blog__: http://blog.actor-framework.org
* __Doxygen (HTML)__: http://www.actor-framework.org/doc
* __Manual (HTML)__: http://www.actor-framework.org/manual
* __Manual (PDF)__: http://www.actor-framework.org/pdf/manual.pdf
* __Mailing List__: https://groups.google.com/d/forum/actor-framework

## Get CAF

### Linux Packages

We provide binary packages for several Linux distributions using the
*openSUSE Build Service*. Please follow the linked installation guides below
or alternatively visit our OBS project homepage:
https://build.opensuse.org/package/show/devel:libraries:caf/caf

package   | description          | link 
----------|----------------------|-----------------------------
caf       | binaries only        | [stable][obs]     [nightly][obs-nightly]
caf-devel | binaries and headers | [stable][obs-dev] [nightly][obs-dev-nightly]


### FreeBSD Ports

We maintain a port for CAF, which you can install as follows:

```sh
pkg install caf
```

Alternatively, you can go to `/usr/ports/devel/caf` and tweak a few
configuration options before installing the port:

```sh
make config
make install clean
```

### Homebrew

You can install the latest stable release with:

```sh
brew install caf
```

Alternatively, you can use the development branch by using:

```sh
brew install caf --HEAD
```

### Biicode

The official CAF channel is published under
[caf_bot/actor-framework](https://www.biicode.com/caf_bot/actor-framework)
and includes the following blocks:

* libcaf_core
* libcaf_io
* libcaf_riac
* libcaf_opencl (*depends on OpenCL which is not distributed as part of CAF*)

**NOTE:** You do not need to have CAF installed on your
machine. Biicode will automatically do that for you during the build process.
Visit this the [bii guide](http://docs.biicode.com/c++/gettingstarted.html)
for more information.

To use actor-framework in your project, reference the header file as:
`#include "caf_bot/actor-framework/libcaf_core/caf/all.hpp"`. Then run
`bii find` to resolve and download the files and `bii build`
to compile your code.

To avoid specifying the block name in your includes add the following to your
`bii.conf` file to allow Biicode to associate all `#include "caf/*.hpp"`
with the actor-framework block:

```ini
[includes]
  caf/riac/*.hpp : caf_bot/actor-framework/libcaf_riac
  caf/opencl/*.hpp : caf_bot/actor-framework/libcaf_opencl
  caf/io/*.hpp : caf_bot/actor-framework/libcaf_io
  caf/*.hpp : caf_bot/actor-framework/libcaf_core
```

## Get the Sources

* git clone https://github.com/actor-framework/actor-framework.git
* cd actor-framework

## Build CAF from Source

The easiest way to build CAF is to use the `configure` script. Other available
options are using [CMake](http://www.cmake.org/) directly or
[SNocs](https://github.com/airutech/snocs).

### Using the `configure` Script

The script is a convenient frontend for `CMake`. See `configure -h`
for a list of available options or read the
[online documentation]
(https://github.com/actor-framework/actor-framework/wiki/Configure-Options).

```sh
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

```latex
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

[obs]:
https://software.opensuse.org/download.html?project=devel%3Alibraries%3Acaf&package=caf

[obs-nightly]:
https://software.opensuse.org/download.html?project=devel%3Alibraries%3Acaf%3Anightly&package=caf

[obs-dev]:
https://software.opensuse.org/download.html?project=devel%3Alibraries%3Acaf&package=caf-devel

[obs-dev-nightly]:
https://software.opensuse.org/download.html?project=devel%3Alibraries%3Acaf%3Anightly&package=caf-devel

[jenkins-badge]:
http://mobi39.cpt.haw-hamburg.de/buildStatus/icon?job=CAF/master%20branch

[coverity-badge]:
https://scan.coverity.com/projects/5555/badge.svg?flat=1

[gitter-badge]:
https://img.shields.io/badge/gitter-join%20chat%20%E2%86%92-brightgreen.svg

[in-progress-badge]:
https://badge.waffle.io/actor-framework/actor-framework.png?label=In%20Progress&title=Issues%20in%20progress

[fixed-in-develop-badge]:
https://badge.waffle.io/actor-framework/actor-framework.png?label=Fixed%20in%20develop&title=Issues%20fixed%20in%20develop
