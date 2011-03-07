CXX = /opt/local/bin/g++-mp-4.5
#CXX = /opt/local/bin/g++-mp-4.6
#CXXFLAGS = -std=c++0x -pedantic -Wall -Wextra -g -O0 -I/opt/local/include/
CXXFLAGS = -std=c++0x -pedantic -Wall -Wextra -O2 -I/opt/local/include/
LIBS = -L/opt/local/lib -lboost_thread-mt
INCLUDES = -I./

EXECUTABLE = test

HEADERS = cppa/test.hpp

SOURCES = src/main.cpp \
		  src/test__a_matches_b.cpp \
		  src/test__atom.cpp \
		  src/test__intrusive_ptr.cpp \
		  src/test__queue_performance.cpp \
		  src/test__serialization.cpp \
		  src/test__spawn.cpp \
		  src/test__tuple.cpp \
		  src/test__type_list.cpp

OBJECTS = $(SOURCES:.cpp=.o)

%.o : %.cpp $(HEADERS) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(EXECUTABLE) : $(OBJECTS) $(HEADERS)
	$(CXX) $(LIBS) -L./ -lcppa $(OBJECTS) -o $(EXECUTABLE)

all : $(EXECUTABLE)

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)

