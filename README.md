# CAF: C++ Actor Framework

CAF is an open source C++11 actor model implementation featuring
lightweight & fast actor implementations, pattern matching for messages,
network transparent messaging, and more.

[![Gitter][gitter-badge]](https://gitter.im/actor-framework/chat)
[![Jenkins][jenkins-badge]](https://jenkins.inet.haw-hamburg.de/job/CAF/job/actor-framework/job/master)
[![Documentation Status][docs-badge]](http://actor-framework.readthedocs.io/en/latest/?badge=latest)
[![Coverity][coverity-badge]](https://scan.coverity.com/projects/5555)
[![In Progress][in-progress-badge]](https://waffle.io/actor-framework/actor-framework)

## Online Resources

* __Homepage__: http://www.actor-framework.org
* __Developer Blog__: http://blog.actor-framework.org
* __Doxygen__: http://www.actor-framework.org/doc
* __HTML Manual__: https://actor-framework.readthedocs.io
* __PDF Manual__: http://www.actor-framework.org/pdf/manual.pdf

## Report Bugs / Get Help

* __Open Issues on GitHub__: https://github.com/actor-framework/actor-framework/issues/new
* __Ask Questions on StackOverflow__: https://stackoverflow.com/questions/ask?tags=c%2b%2b-actor-framework

## Community

* __Chat__: https://gitter.im/actor-framework/chat
* __Twitter__: https://twitter.com/actor_framework
* __User Mailing List__: https://groups.google.com/d/forum/actor-framework
* __Developer Mailing List__: https://groups.google.com/d/forum/caf-devel
* __Feature Proposals__: https://github.com/actor-framework/evolution

## Get CAF

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

Alternatively, you can use the current development version by using:

```sh
brew install caf --HEAD
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

* GCC >= 4.8.3
* Clang >= 3.2
* MSVC >= 2015, update 3

## Supported Operating Systems

* Linux
* Mac OS X
* FreeBSD 10
* Windows >= 7 (currently static builds only)

## Optional Dependencies

* Doxygen (for the `doxygen` target)
* LaTeX (for the `manual` target)
* Pandoc and Python with pandocfilters (for the `rst` target)

## Scientific Use

If you use CAF in a scientific context, please use one of the following citations:

```latex
@inproceedings{cshw-nassp-13,
  author = {Dominik Charousset and Thomas C. Schmidt and Raphael Hiesgen and Matthias W{\"a}hlisch},
  title = {{Native Actors -- A Scalable Software Platform for Distributed, Heterogeneous Environments}},
  booktitle = {Proc. of the 4rd ACM SIGPLAN Conference on Systems, Programming, and Applications (SPLASH '13), Workshop AGERE!},
  pages = {87--96},
  month = {Oct.},
  year = {2013},
  publisher = {ACM},
  address = {New York, NY, USA}
}

@article{chs-rapc-16,
  author = {Dominik Charousset and Raphael Hiesgen and Thomas C. Schmidt},
  title = {{Revisiting Actor Programming in C++}},
  journal = {Computer Languages, Systems \& Structures},
  volume = {45},
  year = {2016},
  month = "April",
  pages = {105--131},
  publisher = {Elsevier}
}
```

You can find the papers online at
http://dx.doi.org/10.1145/2541329.2541336 and
http://dx.doi.org/10.1016/j.cl.2016.01.002.

[obs]: https://software.opensuse.org/download.html?project=devel%3Alibraries%3Acaf&package=caf

[obs-nightly]: https://software.opensuse.org/download.html?project=devel%3Alibraries%3Acaf%3Anightly&package=caf

[obs-dev]: https://software.opensuse.org/download.html?project=devel%3Alibraries%3Acaf&package=caf-devel

[obs-dev-nightly]: https://software.opensuse.org/download.html?project=devel%3Alibraries%3Acaf%3Anightly&package=caf-devel

[jenkins-badge]: https://jenkins.inet.haw-hamburg.de/buildStatus/icon?job=CAF/actor-framework/master

[docs-badge]: https://readthedocs.org/projects/actor-framework/badge/?version=latest

[coverity-badge]: https://scan.coverity.com/projects/5555/badge.svg?flat=1

[gitter-badge]: https://img.shields.io/badge/gitter-join%20chat%20%E2%86%92-brightgreen.svg

[in-progress-badge]: https://badge.waffle.io/actor-framework/actor-framework.png?label=In%20Progress&title=Issues%20in%20progress
