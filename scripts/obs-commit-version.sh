#!/bin/bash

# obs-commit-version.sh: update CAF packages on OBS from Jenkins.
# This shell script is created for easy updating CAF binary packages located
# on OpenSUSE Build Service (OBS). It should be issued from the Jenkins after
# release has proven to be operating, like this:
#
#     $ make
#     $ make test
#     $ make doc                          <-- this is a necessary step!
#     $ obs-commit-version.sh --release   <-- for release build
#     -or-
#     $ obs-commit-version.sh --nightly   <-- for nightly build
#
# In brief, it performs the following steps:
#
#  1. Determines current version from <caf/config.h> header. It's crucial to
#     have this version set correctly for script to operate properly.
#  2. If was run in '--nightly' mode, ask Git to describe current position on
#     the revision tree related to determined stable version.
#  3. Checks out OBS project using "osc" tool. It uses default credentials
#     specified in ~/.oscrc configuration file.
#  4. Puts new source tarball into OBS project.
#  5. Updates version in various spec files.
#  6. Commits changes files to trigger new OBS build.
#
# Copyright (c) 2015, Pavel Kretov <firegurafiku@gmail.com>
#
# Distributed under the terms and conditions of the BSD 3-Clause License or
# (at your option) under the terms and conditions of the Boost Software
# License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.
#
# If you did not receive a copy of the license files, see
# http://opensource.org/licenses/BSD-3-Clause and
# http://www.boost.org/LICENSE_1_0.txt.

set -v
set -o nounset
set -o errexit

# check for `osc` command
type osc > /dev/null 2>&1 || {
  echo "This script requires 'osc', please install it first." >&2
  exit 1
}

# check if header exists
if [ ! -f "libcaf_core/caf/config.hpp" ] ; then
  echo "This script must be called from the root directly of CAF." >&2
  exit 1
fi

# check arguments
if [ $# -ne 2 ] || [ $1 != "--release" -a $1 != "--nightly" ] ; then
  echo "Usage: $0 (--release|--nightly) NAMESPACE:PROJECT/PACKAGE" >&2
  exit 1
fi

operatingMode="$1"
packageFqn="$2"
projectName="$(dirname "$packageFqn")"
packageName="$(basename "$packageFqn")"

sourceDir="$PWD"

# 1.
versionAsInt=$(grep "#define CAF_VERSION" libcaf_core/caf/config.hpp | awk '{print $3'})
versionMajor=$(echo "$versionAsInt / 10000" | bc)
versionMinor=$(echo "( $versionAsInt / 100 ) % 100" | bc)
versionPatch=$(echo "$versionAsInt % 100" | bc)
versionAsStr="$versionMajor.$versionMinor.$versionPatch"

# 2.
version=$versionAsStr
if [ "$operatingMode" = "--nightly" ] ; then
  version="${versionAsStr}-$(date +%Y%m%d)"
fi

# 3, 4
buildDir="$sourceDir/build"
obsDir="$buildDir/obs-temp"
if [ -d "$obsDir" ]; then
  rm -rf "$obsDir"
fi
mkdir -p "$obsDir"
cd "$obsDir"
osc checkout "$packageFqn"
cd "$packageFqn"
osc remove *.tar.gz

cd "$sourceDir"

echo "generate tarball: ${version}.tar.gz"

sourceTarball="${version}.tar.gz"
tar czf "$obsDir/$packageFqn/$sourceTarball" --exclude ".git" --exclude "build" *
cd -

osc add "$sourceTarball"

# 5.
sed -i.bk -E -e "s/^Version:([ ]+).+/Version:\1$version/g" "$packageName.spec"

# 6.
osc commit -m "Automatic commit: $version, $operatingMode"

