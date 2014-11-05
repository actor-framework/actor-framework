#!/bin/bash

host=hylos.cpt.haw-hamburg.de
htmlroot=/prog/www/w3/www.actor-framework.org/html

echo "build doxygen target"
make -C build/ doc || ninja -C build/ doc || exit

echo "build manual"
make -C manual || exit

echo "upload doxygen documentation"
rsync -r -v -u --delete --exclude=.DS_Store html/ $host:$htmlroot/doc || exit

echo "upload HTML manual"
scp manual/build-html/manual.html $host:$htmlroot/manual/index.html || exit

echo "upload PDF manual"
scp manual/build-pdf/manual.pdf $host:$htmlroot/pdf || exit

echo "done"
