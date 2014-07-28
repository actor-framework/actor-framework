#!/bin/bash

echo "check for folders ..."
if [ ! -d manual ]; then
    echo "no manual folder found"
    exit
fi

if [ ! -d ../gh-pages ]; then
    echo "no gh-pages folder found"
    exit
fi

echo "build documentation ..."
make doc &>/dev/null || ninja -C build doc &>/dev/null || exit

if [ ! -f manual/manual.pdf ]; then
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
cp -R ../actor-framework/html/* .
mkdir manual
cp ../actor-framework/manual/manual.pdf manual/
cp ../actor-framework/manual/manual.html manual/index.html

echo "commit ..."
git add .
git commit -a -m "documentation update"

echo "push ..."
git push

cd ../actor-framework
