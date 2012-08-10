#!/bin/bash

echo "build documentation ..."
make doc &>/dev/null

if [ -f manual.pdf ]; then
    echo "PDF manual found ..."
else
    echo "no PDF manual found ... stop"
    exit
fi

echo "build HTML manual ..."
cd manual/
# runs hevea three times
make html &>/dev/null

echo "copy documentation into gh-pages ..."
cd ../../gh-pages
rm -f *.tex *.html *.css *.png *.js manual/manual.pdf manual/index.html
cp -R ../libcppa/html/* .
mkdir manual
cp ../libcppa/manual.pdf manual/
cp ../libcppa/manual/manual.html manual/index.html

echo "commit ..."
git add .
git commit -a -m "documentation update"

echo "push ..."
git push

cd ../libcppa
