#!/bin/bash

blacklist="manual.tex variables.tex colors.tex newcommands.tex"

inBlacklist() {
  for e in $blacklist; do
    [[ "$e" == "$1" ]] && return 0
  done
  return 1
}

for i in *.tex ; do
  if inBlacklist $i ; then
    continue
  fi
  out_file=$(basename $i .tex)
  echo "$i => $out_file"
  cat colors.tex variables.tex newcommands.tex $i | ./explode_lstinputlisting.py | pandoc --filter=$PWD/filter.py --wrap=none --listings -f latex -o ${out_file}.rst
done
sphinx-build -b html . html
