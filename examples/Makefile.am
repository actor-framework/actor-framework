AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = -I ../m4

AM_CXXFLAGS = -I../ --std=c++0x -pedantic -Wall -Wextra

noinst_PROGRAMS = announce_example_1 \
                  announce_example_2 \
                  announce_example_3 \
                  announce_example_4 \
                  announce_example_5 \
                  hello_world_example \
                  math_example

announce_example_1_SOURCES = announce_example_1.cpp
announce_example_2_SOURCES = announce_example_2.cpp
announce_example_3_SOURCES = announce_example_3.cpp
announce_example_4_SOURCES = announce_example_4.cpp
announce_example_5_SOURCES = announce_example_5.cpp
hello_world_example_SOURCES = hello_world_example.cpp
math_example_SOURCES = math_example.cpp

EXAMPLES_LIBS = $(BOOST_LDFLAGS) $(BOOST_THREAD_LIB) -L../.libs/ -lcppa

announce_example_1_LDADD = $(EXAMPLES_LIBS)
announce_example_2_LDADD = $(EXAMPLES_LIBS)
announce_example_3_LDADD = $(EXAMPLES_LIBS)
announce_example_4_LDADD = $(EXAMPLES_LIBS)
announce_example_5_LDADD = $(EXAMPLES_LIBS)
hello_world_example_LDADD = $(EXAMPLES_LIBS)
math_example_LDADD = $(EXAMPLES_LIBS)

