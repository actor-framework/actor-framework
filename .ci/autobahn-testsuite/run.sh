#! /bin/sh


echo "This is a very hard test"

HERE=$(dirname "$0")

ls "$HERE"
ls "$HERE/.."
ls "$HERE/../.."

exit 1
