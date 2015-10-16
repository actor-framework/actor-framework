#!/bin/bash

# obs-commit-version.sh: update CAF packages on OBS from Jenkins.
# This shell script is created for easy updating CAF binary packages located
# on OpenSUSE Build Service (OBS). It should be issued from the Jenkins after
# release has proven to be operating, like this:
#
#     $ make
#     $ make test
#     $ make doc                       <-- this is a necessary step!
#     $ scripts/obs-commit-version.sh
#
# In brief, it performs the following steps:
#
#  1. Determines current version from <caf/config.h> header. It's crucial to
#     have this version set correctly for script to operate properly.
#  2. Determines current branch. If it's 'master' then rebuild stable
#     packages, if it's 'develop' then rebuild nightly.
#  3. Checks out OBS project using "osc" tool. It uses default credentials
#     specified in ~/.oscrc configuration file.
#  4. Puts new source tarball into OBS project.
#  5. Updates version in various spec files.
#  6. Commits changes files to trigger new OBS build.
#
# Copyright (c) 2015, Pavel Kretov <firegurafiku@gmail.com>
# Copyright (c) 2015, Dominik Charousset <dominik.charousset@haw-hamburg.de>
#
# Distributed under the terms and conditions of the BSD 3-Clause License or
# (at your option) under the terms and conditions of the Boost Software
# License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.
#
# If you did not receive a copy of the license files, see
# http://opensource.org/licenses/BSD-3-Clause and
# http://www.boost.org/LICENSE_1_0.txt.

set -o nounset
set -o errexit

# Script configuration (yet unlikely to change in the future).
confReleaseBranch="master"
confNightlyBranch="develop"
confReleaseProject="devel:libraries:caf"
confNightlyProject="devel:libraries:caf:nightly"
confReleasePackage="caf"
confNightlyPackage="caf"

# Check for 'osc' command.
type osc >/dev/null 2>&1 || {
  echo "This script requires 'osc', please install it first." >&2
  exit 1
}

sourceDir="$PWD"

# Check if header exists.
if [ ! -f "$sourceDir/libcaf_core/caf/config.hpp" ] ; then
  echo "This script must be called from the root directly of CAF." >&2
  exit 1
fi

if [ ! -d "$sourceDir/html" \
  -o ! -f "$sourceDir/manual/manual.html"\
  -o ! -f "$sourceDir/manual/manual.pdf" ] ; then
  echo "Documentation must be generated before calling this script." >&2
  exit 1
fi

# Extract version information from header file.
versionAsInt=$(grep "#define CAF_VERSION" libcaf_core/caf/config.hpp | awk '{print $3'})
versionMajor=$(echo "$versionAsInt / 10000" | bc)
versionMinor=$(echo "( $versionAsInt / 100 ) % 100" | bc)
versionPatch=$(echo "$versionAsInt % 100" | bc)
versionAsStr="$versionMajor.$versionMinor.$versionPatch"

# Retrieve current branch from git.
gitBranch=`git rev-parse --abbrev-ref HEAD`

if [ "$gitBranch" = "$confReleaseBranch" ] ; then 
  projectName="$confReleaseProject"
  packageName="$confReleasePackage"
  packageVersion="${versionAsStr}"
elif [ "$gitBranch" = "$confNightlyBranch" ] ; then
  projectName="$confNightlyProject"
  packageName="$confNightlyPackage"
  packageVersion="${versionAsStr}_$(date +%Y%m%d)"
else
  # Don't prevent other branches from building, but issue a warning.
  echo "Not on '$confReleaseBranch' or '$confNightlyBranch' branch. Exitting." >&2
  exit
fi

packageFqn="$projectName/$packageName"
sourceTarball="${versionAsStr}.tar.gz"
buildDir="$sourceDir/build"
obsDir="$buildDir/obs-temp"

# Prepare a directory for checkout.
[ -d "$obsDir" ] && rm -rf "$obsDir"
mkdir -p "$obsDir"
cd "$obsDir"

# Checkout the project from OBS into a newly created directory.
echo "[obs-commit-version] Checking out"
osc checkout "$packageFqn"

# Remove old tarball.
cd "$packageFqn"
osc remove *.tar.gz

# And generate a new tarball.
cd "$sourceDir"
echo "[obs-commit-version] Generating tarball: ${versionAsStr}.tar.gz"
tar czf "$obsDir/$packageFqn/$sourceTarball" --exclude ".git" --exclude "build" *
cd - >/dev/null

osc add "$sourceTarball"

# Fix package version and commit.
sed -i.bk -E -e "s/^Version:([ ]+).+/Version:\1$packageVersion/g" "$packageName.spec"
echo "[obs-commit-version] Comitting: $packageVersion, $gitBranch"
osc commit -m "Automatic commit: $packageVersion, $gitBranch"

