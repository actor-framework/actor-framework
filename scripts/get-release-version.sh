#!/bin/sh

# Get string representation of CAF version.
caf_version=`grep "define CAF_VERSION" libcaf_core/caf/config.hpp | awk '{ printf "%d.%d.%d", int($3 / 10000), int($3 / 100) % 100, $3 % 100 }'`

# Get SHA from Git.
git_sha=`git log --pretty=format:%h -n 1`

# Check whether the current SHA is a tag.
if git describe --tags --contains $git_sha 1>release.txt 2>/dev/null
then
  # Tags indicate stable release -> use tag version.
  # On success, we'll have the tag version in release.txt now, so we're done.
  exit 0
fi

# Generate default release version.
echo "$caf_version+exp.sha.$git_sha" >release.txt
