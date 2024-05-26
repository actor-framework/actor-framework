# CAF: the C++ Actor Framework

CAF is an open source implementation of the actor model for C++ featuring
lightweight & fast actor implementations, pattern matching for messages, network
transparent messaging, and more.

[![Gitter][gitter-badge]](https://gitter.im/actor-framework/chat)
[![Documentation Status][docs-badge]](http://actor-framework.readthedocs.io/en/latest/?badge=latest)
[![Coverage][coverage-badge]](https://codecov.io/gh/actor-framework/actor-framework)

## Online Resources

* __Homepage__: https://www.actor-framework.org
* __Developer Blog__: https://www.actor-framework.org/blog
* __Guides and Tutorials__: https://www.cafcademy.com/articles
* __Manual__: https://actor-framework.readthedocs.io
* __Doxygen__: https://www.actor-framework.org/doxygen/

## Report Bugs / Get Help

* __Open Issues on GitHub__: https://github.com/actor-framework/actor-framework/issues/new
* __Ask Questions on StackOverflow__: https://stackoverflow.com/questions/ask?tags=c%2b%2b-actor-framework

## Community

* __Chat__: https://gitter.im/actor-framework/chat
* __Mastodon__: https://fosstodon.org/@caf
* __Twitter__: https://twitter.com/actor_framework
* __User Mailing List__: https://groups.google.com/d/forum/actor-framework

## Get CAF

We do not officially maintain packages for CAF. However, some of our community
members made packages available for these package managers:

- [Conan](https://conan.io/center/caf)
- [FreeBSD Ports](https://svnweb.freebsd.org/ports/head/devel/caf)
- [Homebrew](https://formulae.brew.sh/formula/caf).
- [VCPKG](https://github.com/microsoft/vcpkg/tree/master/ports/caf)

## Get the Sources

```sh
git clone https://github.com/actor-framework/actor-framework.git
cd actor-framework
```

## Build CAF from Source

CAF uses [CMake](http://www.cmake.org) as its build system of choice. To make
building CAF more convenient from the command line, we provide a `configure`
script that wraps the CMake invocation. The script only works on UNIX systems.
On Windows, we recommend generating an MSVC project file via CMake for native
builds.

### Using the `configure` Script

The script is a convenient frontend for `CMake`. See `configure -h` for a list
of available options. By default, the script creates a `build` directory and
asks CMake to generate a `Makefile`. A build with default settings generally
follows these steps:

```sh
./configure
cd build
make
make test [optional]
make install [as root, optional]
```

### Using CMake

To generate a Makefile for building CAF with default settings, either use a
CMake GUI or perform these steps on the command line:

```sh
mkdir build
cd build
cmake ..
```

After this step, `cmake -LH` prints the most useful configuration options for
CAF, their default value, and a helptext.

Other CMake projects can add CAF as a dependency by using `find_package` and
listing the required modules (e.g., `core` or `io`) . When installing CAF to a
non-standard location, set `CAF_ROOT` prior to calling `find_package`.

## Dependencies

* CMake (for building CAF)
* OpenSSL (only when building the OpenSSL module)

## Supported Platforms

C++ is an evolving language. Compiler vendors constantly add more language and
standard library features. Since CAF runs on many platforms, this means we need
a policy that on the one hand ensures that we only use a widely supported subset
of C++ and on the other hand that we naturally progress with the shifting
landscape to eventually catch up to more recent C++ additions (in order to not
"get stuck").

So instead of singling out individual compiler versions, we build CAF for each
commit on all platforms that we currently deem relevant. Everything that passes
our CI is "fair game".

Our CI covers Windows (latest MSVC release), macOS (latest Xcode release),
FreeBSD (latest) and several Linux distributions (via the Docker images found
[here](https://github.com/actor-framework/actor-framework/tree/main/.ci)). For
Linux, we aim to support the current releases (that still receive active
support) for the major distributions. Note that we do not build on Linux
distributions with rolling releases, because they generally provide more recent
build tools than distributions with traditional release schedules and thus would
only add redundancy.

## Build Documentation Locally

- Building an offline version of the manual requires
  [Sphinx](https://www.sphinx-doc.org):
  ```sh
  cd manual
  sphinx-build . html
  ```
- Building an offline version of the API reference documentation requires
  [Doxygen](http://www.doxygen.nl) (simply run the  `doxygen` command at the
  root directory of the repository).

## Scientific Use

If you use CAF in a scientific context, please use one of the following citations:

```bibtex
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
  month = {April},
  pages = {105--131},
  publisher = {Elsevier}
}
```

You can find the papers online at http://dx.doi.org/10.1145/2541329.2541336 and
http://dx.doi.org/10.1016/j.cl.2016.01.002.

[docs-badge]: https://readthedocs.org/projects/actor-framework/badge/?version=latest

[gitter-badge]: https://img.shields.io/badge/gitter-join%20chat%20%E2%86%92-brightgreen.svg

[coverage-badge]: https://codecov.io/gh/actor-framework/actor-framework/graph/badge.svg?token=SjJQQ5dCbV
