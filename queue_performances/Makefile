include ../Makefile.rules

#CXX = /opt/local/bin/g++-mp-4.5
#CXX = /opt/local/bin/g++-mp-4.6
#CXXFLAGS = -std=c++0x -pedantic -Wall -Wextra -g -O0 -I/opt/local/include/
#CXXFLAGS = -std=c++0x -pedantic -Wall -Wextra -O2 -I/opt/local/include/
#LIBS = -L/opt/local/lib -lboost_thread-mt -L../ -lcppa
INCLUDES = -I./ -I../

FLAGS = -DCACHE_LINE_SIZE=64

EXECUTABLE = ../queue_test

HEADERS = sutter_list.hpp

SOURCES =  main.cpp

OBJECTS = $(SOURCES:.cpp=.o)

%.o : %.cpp $(HEADERS) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(FLAGS) -c $< -o $@

$(EXECUTABLE) : $(OBJECTS) $(HEADERS)
	$(CXX) $(LIBS) -L../ -lcppa $(OBJECTS) -o $(EXECUTABLE)

all : $(EXECUTABLE)

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)

