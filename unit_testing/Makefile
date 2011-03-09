CXX = /opt/local/bin/g++-mp-4.5
#CXX = /opt/local/bin/g++-mp-4.6
#CXXFLAGS = -std=c++0x -pedantic -Wall -Wextra -g -O0 -I/opt/local/include/
CXXFLAGS = -std=c++0x -pedantic -Wall -Wextra -O2 -I/opt/local/include/
LIBS = -L/opt/local/lib -lboost_thread-mt -L../ -lcppa
INCLUDES = -I./ -I../

EXECUTABLE = ../test

HEADERS = test.hpp

SOURCES = hash_of.cpp \
		  main.cpp \
		  test__a_matches_b.cpp \
		  test__atom.cpp \
		  test__intrusive_ptr.cpp \
		  test__local_group.cpp \
		  test__queue_performance.cpp \
		  test__serialization.cpp \
		  test__spawn.cpp \
		  test__tuple.cpp \
		  test__type_list.cpp

OBJECTS = $(SOURCES:.cpp=.o)

%.o : %.cpp $(HEADERS) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(EXECUTABLE) : $(OBJECTS) $(HEADERS)
	$(CXX) $(LIBS) -L./ -lcppa $(OBJECTS) -o $(EXECUTABLE)

all : $(EXECUTABLE)

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)

