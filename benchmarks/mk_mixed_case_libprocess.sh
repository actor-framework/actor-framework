#!/bin/bash

# quick & dirty: eval args to enable CXX=... and CXXFLAGS=...
for arg in "$@"; do
    eval "$arg"
done

if test "" = "$CXX"; then
    CXX=g++
fi

function verbose_exec {
    echo $1
    eval $1
    if test "0" != "$?" ; then
        echo ; echo "command failed!"
        exit
    fi 
}

function check_var {
    if test "" = "$1" ; then
        echo "something went wrong ... $2 not found"
        exit
    fi
}

function fetch_lib {
  find mesos/third_party/libprocess -name "$1"
  if test "$2" = "true" ; then
    echo "something went wrong ... $1 not found"
  fi
}

if ! test -d mesos ; then
    echo "fetch mesos (for libprocess)"
    verbose_exec "git clone https://github.com/apache/mesos.git"
else
    echo "found mesos repository"
fi

LIBFILE=$(fetch_lib libprocess.a)
LIBGLOGFILE=$(fetch_lib libglog.a)
LIBEVFILE=$(fetch_lib libev.a)

echo "LIBFILE=$LIBFILE"
echo "LIBGLOGFILE=$LIBGLOGFILE"
echo "LIBEVFILE=$LIBEVFILE"

if test "" = "$LIBFILE" -o "" = "$LIBGLOGFILE" -o "" = "$LIBEVFILE" ; then
    CURR=$PWD
    echo "build libprocess"
    cd mesos/third_party/libprocess/
    verbose_exec "autoreconf -Wnone -i && ./configure CXX=\"$CXX\" CXXFLAGS=\"$CXXFLAGS -D_XOPEN_SOURCE\" && make"
    echo "build third_party libraries"
    cd third_party
    for dir in * ; do
        if test -d $dir ; then
            # skip boost in third_party directory
            if [[ "$dir" == glog* ]] || [[ "$dir" == libev* ]] ; then
                echo "cd $dir"
                cd $dir
                verbose_exec "autoreconf -i ; ./configure CXX=\"$CXX\" CXXFLAGS=\"$CXXFLAGS\" ; make"
                echo "cd .."
                cd ..
            fi
        fi
    done
    cd $CURR
    LIBFILE=$(fetch_lib libprocess.a true)
    LIBGLOGFILE=$(fetch_lib libglog.a true)
    LIBEVFILE=$(fetch_lib libev.a true)
else
    echo "found libprocess library: $LIBFILE"
fi
LIBDIRS="-L$(dirname "$LIBFILE") -L$(dirname "$LIBGLOGFILE") -L$(dirname "$LIBEVFILE")"

verbose_exec "$CXX --std=c++0x $CXXFLAGS -Imesos/third_party/libprocess/include/ $LIBDIRS mixed_case_libprocess.cpp -o mixed_case_libprocess -lprocess -lglog -lev"

