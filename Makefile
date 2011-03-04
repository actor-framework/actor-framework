#all:
#	/opt/local/bin/g++-mp-4.5 -std=c++0x -g -O0 main.cpp

CXX = /opt/local/bin/g++-mp-4.5
#CXX = /opt/local/bin/g++-mp-4.6
CXXFLAGS = -std=c++0x -pedantic -Wall -Wextra -g -O0
LIBS =
INCLUDES = -I./
HEADERS = cppa/actor.hpp \
		  cppa/any_type.hpp \
		  cppa/config.hpp \
		  cppa/cow_ptr.hpp \
		  cppa/get.hpp \
		  cppa/intrusive_ptr.hpp \
		  cppa/invoke.hpp \
		  cppa/invoke_rules.hpp \
		  cppa/match.hpp \
		  cppa/message.hpp \
		  cppa/on.hpp \
		  cppa/ref_counted.hpp \
		  cppa/test.hpp \
		  cppa/tuple.hpp \
		  cppa/tuple_view.hpp \
		  cppa/uniform_type_info.hpp \
		  cppa/untyped_tuple.hpp \
		  cppa/util.hpp \
		  cppa/detail/abstract_tuple.hpp \
		  cppa/detail/decorated_tuple.hpp \
		  cppa/detail/intermediate.hpp \
		  cppa/detail/invokable.hpp \
		  cppa/detail/matcher.hpp \
		  cppa/detail/ref_counted_impl.hpp \
		  cppa/detail/scheduler.hpp \
		  cppa/detail/tdata.hpp \
		  cppa/detail/tuple_vals.hpp \
		  cppa/util/a_matches_b.hpp \
		  cppa/util/callable_trait.hpp \
		  cppa/util/compare_tuples.hpp \
		  cppa/util/concat_type_lists.hpp \
		  cppa/util/conjunction.hpp \
		  cppa/util/detach.hpp \
		  cppa/util/disjunction.hpp \
		  cppa/util/eval_type_lists.hpp \
		  cppa/util/filter_type_list.hpp \
		  cppa/util/has_copy_member_fun.hpp \
		  cppa/util/is_comparable.hpp \
		  cppa/util/is_copyable.hpp \
		  cppa/util/is_one_of.hpp \
		  cppa/util/remove_const_reference.hpp \
		  cppa/util/reverse_type_list.hpp \
		  cppa/util/type_at.hpp \
		  cppa/util/type_list.hpp \
		  cppa/util/type_list_apply.hpp \
		  cppa/util/type_list_pop_back.hpp \
		  cppa/util/utype_iterator.hpp \
		  cppa/util/void_type.hpp
SOURCES = src/decorated_tuple.cpp \
		  src/deserializer.cpp \
		  src/main.cpp \
		  src/ref_counted.cpp \
		  src/serializer.cpp \
		  src/test__a_matches_b.cpp \
		  src/test__atom.cpp \
		  src/test__intrusive_ptr.cpp \
		  src/test__serialization.cpp \
		  src/test__spawn.cpp \
		  src/test__tuple.cpp \
		  src/test__type_list.cpp \
		  src/uniform_type_info.cpp \
		  src/untyped_tuple.cpp
OBJECTS = $(SOURCES:.cpp=.o)
EXECUTABLE = test

%.o : %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(EXECUTABLE) : $(OBJECTS) $(HEADERS)
	$(CXX) $(LIBS) $(OBJECTS) -o $(EXECUTABLE)

all : $(SOURCES) $(EXECUTABLE)

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)
