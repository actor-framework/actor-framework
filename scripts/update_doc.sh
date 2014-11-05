#!/bin/bash

rsync -r -v -u --delete --exclude=.DS_Store html/ hylos.cpt.haw-hamburg.de:/prog/www/w3/www.actor-framework.org/html/doc
