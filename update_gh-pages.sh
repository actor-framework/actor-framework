#!/bin/bash

doxygen libcppa.doxygen
cd ../gh-pages
rm -f *.tex *.html *.css *.png *.js
cp -R ../libcppa/html/* .
git add .
git commit -a -m "documentation update"
git push
cd ../libcppa
