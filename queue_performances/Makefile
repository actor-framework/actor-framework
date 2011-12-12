CXX = /usr/bin/g++-4.6
CXXFLAGS = -std=c++0x -pedantic -Wall -Wextra -O2

#CXX = /opt/local/bin/g++-mp-4.5
#CXX = /opt/local/bin/g++-mp-4.6
#CXXFLAGS = -std=c++0x -pedantic -Wall -Wextra -g -O0 -I/opt/local/include/
#CXXFLAGS = -std=c++0x -pedantic -Wall -Wextra -O2 -I/opt/local/include/
#LIBS = -L/opt/local/lib -lboost_thread-mt -L../ -lcppa
INCLUDES = -I./

LIBS = -pthread

FLAGS = -DCACHE_LINE_SIZE=64

EXECUTABLE = queue_test

HEADERS = blocking_cached_stack2.hpp  blocking_cached_stack.hpp  blocking_sutter_list.hpp  cached_stack.hpp  defines.hpp  intrusive_sutter_list.hpp  lockfree_list.hpp  sutter_list.hpp

SOURCES =  main.cpp

OBJECTS = $(SOURCES:.cpp=.o)

%.o : %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(FLAGS) -c $< -o $@

$(EXECUTABLE) : $(OBJECTS) $(HEADERS)
	$(CXX) $(LIBS) $(OBJECTS) -o $(EXECUTABLE)

all : $(EXECUTABLE)

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)

