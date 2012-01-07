AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = -I ../m4

AM_CXXFLAGS = -I../ --std=c++0x -pedantic -Wall -Wextra

noinst_PROGRAMS = actor_creation mailbox_performance mixed_case

actor_creation_SOURCES = actor_creation.cpp
mailbox_performance_SOURCES = mailbox_performance.cpp
mixed_case_SOURCES = mixed_case.cpp

EXAMPLES_LIBS = -L../.libs/ -lcppa $(BOOST_LDFLAGS) $(BOOST_THREAD_LIB)

actor_creation_LDADD = $(EXAMPLES_LIBS)
mailbox_performance_LDADD = $(EXAMPLES_LIBS)
mixed_case_LDADD = $(EXAMPLES_LIBS)

