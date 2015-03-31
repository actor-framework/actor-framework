#!/bin/bash

set -e

# minimum iOS version
IOS_DEPLOYMENT_TARGET=${IOS_DEPLOYMENT_TARGET:-"6.0"}

# location of the repository root. should contain configure file
SRCROOT=$( cd "$( dirname "${BASH_SOURCE[0]}" )"/.. && pwd )

if [ ! -f "$SRCROOT/configure" ]; then
    echo "$SRCROOT is not repository root." \
         "check the $(basename ${BASH_SOURCE[0]}$0) file location."
    exit 100
fi

IPHONEOS_BUILD_DIR=build-iphoneos
IPHONESIMULATOR_BUILD_DIR=build-iphonesimulator

PREFIX=${PREFIX:-"${SRCROOT}/dist-ios"}

cd $SRCROOT

# ios simulator

./configure --generator=Xcode --build-dir=${IPHONESIMULATOR_BUILD_DIR} \
    --build-static-only --no-examples --no-unit-tests --sysroot=iphonesimulator \
    --ios-min-ver=${IOS_DEPLOYMENT_TARGET} --prefix=${PREFIX}

xcodebuild -project ${IPHONESIMULATOR_BUILD_DIR}/caf.xcodeproj -target ALL_BUILD \
    -configuration Debug
#  install headers
xcodebuild -project ${IPHONESIMULATOR_BUILD_DIR}/caf.xcodeproj -target install \
    -configuration Release


# iphone os

./configure --generator=Xcode --build-dir=${IPHONEOS_BUILD_DIR} \
    --build-static-only --no-examples --no-unit-tests --sysroot=iphoneos \
    --ios-min-ver=${IOS_DEPLOYMENT_TARGET} --prefix=${PREFIX}

xcodebuild -project ${IPHONEOS_BUILD_DIR}/caf.xcodeproj -target ALL_BUILD \
    -configuration Debug
xcodebuild -project ${IPHONEOS_BUILD_DIR}/caf.xcodeproj -target ALL_BUILD \
    -configuration Release

mkdir -p ${PREFIX}/lib/Debug
lipo -create \
    ${IPHONEOS_BUILD_DIR}/bin/Debug/libcaf_core_static.a \
    ${IPHONESIMULATOR_BUILD_DIR}/bin/Debug/libcaf_core_static.a \
    -output ${PREFIX}/lib/Debug/libcaf_core_static.a
lipo -create \
    ${IPHONEOS_BUILD_DIR}/bin/Debug/libcaf_io_static.a \
    ${IPHONESIMULATOR_BUILD_DIR}/bin/Debug/libcaf_io_static.a \
    -output ${PREFIX}/lib/Debug/libcaf_io_static.a

mkdir -p ${PREFIX}/lib/Release
lipo -create \
    ${IPHONEOS_BUILD_DIR}/bin/Release/libcaf_core_static.a \
    ${IPHONESIMULATOR_BUILD_DIR}/bin/Release/libcaf_core_static.a \
    -output ${PREFIX}/lib/Release/libcaf_core_static.a
lipo -create \
    ${IPHONEOS_BUILD_DIR}/bin/Release/libcaf_io_static.a \
    ${IPHONESIMULATOR_BUILD_DIR}/bin/Release/libcaf_io_static.a \
    -output ${PREFIX}/lib/Release/libcaf_io_static.a

rm -rf ${IPHONEOS_BUILD_DIR}
rm -rf ${IPHONESIMULATOR_BUILD_DIR}
