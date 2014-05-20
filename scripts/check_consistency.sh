#!/bin/bash

command -v pdf2txt.py &>/dev/null 
if [ $? -ne 0 ]; then
    echo "pdf2txt.py not found"
    exit
fi

if [ ! -d manual ]; then
    echo "directory 'manual' does not exist"
    exit
fi

echo "build manual/manual.pdf"
make -C manual pdf &>/dev/null
if [ $? -ne 0 ]; then
    echo "failure during build!"
    exit
fi
echo "read manual.pdf"
# compare PDFs without timestamp
MONTH_RX="(January|February|March|April|May|June|July|August|September|October|November|December)"
PDF1=$(pdf2txt.py manual.pdf | grep -vE "$MONTH_RX")
echo "read manual/manual.pdf"
PDF2=$(pdf2txt.py manual/manual.pdf | grep -vE "$MONTH_RX")
if [ "$PDF1" != "$PDF2" ] ; then
    echo "manual.pdf and manual/manual.pdf differ!"
    exit
else
    echo "manual.pdf up-to-date"
fi

function expand_version_string {
    echo "$1.0" | awk 'BEGIN {FS="."};{printf $1 "." $2 "." $3}';
}

CMAKE_VERSION=$(cat VERSION)
MANUAL_VERSION=$(echo "$PDF1" | grep -oE "version [0-9]+(\.[0-9]+){1,2}" | awk '{print $2}')
CHANGELOG_VERSION=$(head -n1 ChangeLog.md | awk '{print $2}')

MANUAL_VERSION=$(expand_version_string "$MANUAL_VERSION")
CHANGELOG_VERSION=$(expand_version_string "$CHANGELOG_VERSION")

echo "libcppa version in VERSION is $CMAKE_VERSION"
echo "libcppa version in manual.pdf is $MANUAL_VERSION"
echo "libcppa version in ChangeLog is $CHANGELOG_VERSION"

if [ "$CMAKE_VERSION" == "$MANUAL_VERSION" ] && [ "$CMAKE_VERSION" == "$CHANGELOG_VERSION" ] ; then
    echo "no errors found"
else
    echo "versions differ"
fi
