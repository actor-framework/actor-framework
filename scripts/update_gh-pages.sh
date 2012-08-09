#!/bin/bash

echo "build documentation ..."
make doc &>/dev/null

cd ../inetd/libcppa_manual/
if [ -f manual.pdf ]; then
    echo "PDF manual found ..."
else
    echo "no PDF manual found ... stop"
    exit
fi

echo "build HTML manual ..."
# run hevea three times
for i in {1..3} ; do
    hevea manual.tex &>/dev/null 
done

echo "copy documentation into gh-pages ..."
cd ../../gh-pages
rm -rf *.tex *.html *.css *.png *.js manual
cp -R ../libcppa/html/* .
mkdir manual
cp ../inetd/libcppa_manual/manual.pdf manual/
cp ../inetd/libcppa_manual/manual.html manual/index.html

echo "commit ..."
git add .
git commit -a -m "documentation update"

echo "push ..."
git push

cd ../libcppa
