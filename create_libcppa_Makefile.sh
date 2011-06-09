#!/bin/bash

HEADERS=""
SOURCES=""
NLINE="\\n"
BSLASH="\\\\"

function append_hpp_from()
{
	for i in "$1"/*.hpp ; do
		HEADERS="$HEADERS ${BSLASH}${NLINE}          $i"
	done
}

function append_cpp_from()
{
	for i in "$1/"*.cpp ; do
		SOURCES="$SOURCES ${BSLASH}${NLINE}          $i"
	done
}

append_hpp_from "cppa"
append_hpp_from "cppa/detail"
append_hpp_from "cppa/util"

append_cpp_from "src"

echo "include Makefile.rules"
echo "INCLUDE_FLAGS = \$(INCLUDES) -I./"
echo
printf "%b\n" "HEADERS =$HEADERS"
echo
printf "%b\n" "SOURCES =$SOURCES"
echo
echo "OBJECTS = \$(SOURCES:.cpp=.o)"
echo
echo "LIB_NAME = $LIB_NAME"
echo
echo "%.o : %.cpp \$(HEADERS)"
printf "%b\n" "\t\$(CXX) \$(CXXFLAGS) \$(INCLUDE_FLAGS) -fPIC -c \$< -o \$@"
echo
echo "\$libcppa : \$(OBJECTS) \$(HEADERS)"
if test "$(uname)" "=" "Darwin" ; then
	printf "%b\n" "\t\$(CXX) \$(LIBS) -dynamiclib -o libcppa.dylib \$(OBJECTS)"
else
	printf "%b\n" "\t\$(CXX) \$(LIBS) -shared -Wl,-soname,libcppa.so.0 -o libcppa.so.0.0.0 \$(OBJECTS)"
	printf "%b\n" "\tln -s libcppa.so.0.0.0 libcppa.so.0.0"
	printf "%b\n" "\tln -s libcppa.so.0.0.0 libcppa.so.0"
	printf "%b\n" "\tln -s libcppa.so.0.0.0 libcppa.so"
fi
echo
echo "all : libcppa \$(OBJECTS)"
echo
echo "clean:"
printf "%b\n" "\trm -f \$(LIB_NAME) \$(OBJECTS)"

