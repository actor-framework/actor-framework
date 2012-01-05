AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = -I ../m4

AM_CXXFLAGS = -I../ --std=c++0x -pedantic -Wall -Wextra

noinst_PROGRAMS = actor_creation

actor_creation_SOURCES = actor_creation.cpp

EXAMPLES_LIBS = -L../.libs/ -lcppa $(BOOST_LDFLAGS) $(BOOST_THREAD_LIB)

actor_creation_LDADD = $(EXAMPLES_LIBS)

