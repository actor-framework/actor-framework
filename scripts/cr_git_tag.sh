#!/bin/bash

VERSION=$(cat VERSION)
git tag -a "V$VERSION" -m "version $VERSION"
git push --tags
