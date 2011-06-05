include ../Makefile.rules

INCLUDE_FLAGS = $(INCLUDES) -I../ -I./

LIB_FLAGS = $(LIBS) -L../ -lcppa

EXECUTABLE = ../test

HEADERS = hash_of.hpp \
		  test.hpp \
		  ping_pong.hpp

SOURCES = hash_of.cpp \
		  main.cpp \
		  ping_pong.cpp \
		  test__a_matches_b.cpp \
		  test__atom.cpp \
		  test__intrusive_ptr.cpp \
		  test__local_group.cpp \
		  test__primitive_variant.cpp \
		  test__queue_performance.cpp \
		  test__remote_actor.cpp \
		  test__ripemd_160.cpp \
		  test__serialization.cpp \
		  test__spawn.cpp \
		  test__tuple.cpp \
		  test__uniform_type.cpp \
		  test__type_list.cpp

OBJECTS = $(SOURCES:.cpp=.o)

%.o : %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDE_FLAGS) -c $< -o $@

$(EXECUTABLE) : $(OBJECTS) $(HEADERS)
	$(CXX) $(LIB_FLAGS) $(OBJECTS) -o $(EXECUTABLE)

all : $(EXECUTABLE)

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)

