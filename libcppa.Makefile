include Makefile.rules

INCLUDES = -I./

#CXXFLAGS = -std=c++0x -pedantic -Wall -Wextra -O2 -I/opt/local/include/ -fPIC

HEADERS = cppa/actor.hpp \
		  cppa/any_type.hpp \
		  cppa/binary_deserializer.hpp \
		  cppa/binary_serializer.hpp \
		  cppa/channel.hpp \
		  cppa/config.hpp \
		  cppa/context.hpp \
		  cppa/cow_ptr.hpp \
		  cppa/cppa.hpp \
		  cppa/deserializer.hpp \
		  cppa/get.hpp \
		  cppa/group.hpp \
		  cppa/intrusive_ptr.hpp \
		  cppa/invoke.hpp \
		  cppa/invoke_rules.hpp \
		  cppa/match.hpp \
		  cppa/message.hpp \
		  cppa/message_queue.hpp \
		  cppa/object.hpp \
		  cppa/on.hpp \
		  cppa/primitive_type.hpp \
		  cppa/primitive_variant.hpp \
		  cppa/ref_counted.hpp \
		  cppa/scheduler.hpp \
		  cppa/scheduling_hint.hpp \
		  cppa/serializer.hpp \
		  cppa/tuple.hpp \
		  cppa/tuple_view.hpp \
		  cppa/uniform_type_info.hpp \
		  cppa/untyped_tuple.hpp \
		  cppa/util.hpp \
		  cppa/detail/abstract_tuple.hpp \
		  cppa/detail/blocking_message_queue.hpp \
		  cppa/detail/converted_thread_context.hpp \
		  cppa/detail/decorated_tuple.hpp \
		  cppa/detail/channel.hpp \
		  cppa/detail/intermediate.hpp \
		  cppa/detail/invokable.hpp \
		  cppa/detail/matcher.hpp \
		  cppa/detail/ref_counted_impl.hpp \
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
		  cppa/util/is_primitive.hpp \
		  cppa/util/pt_token.hpp \
		  cppa/util/remove_const_reference.hpp \
		  cppa/util/replace_type.hpp \
		  cppa/util/reverse_type_list.hpp \
		  cppa/util/rm_ref.hpp \
		  cppa/util/shared_spinlock.hpp \
		  cppa/util/single_reader_queue.hpp \
		  cppa/util/type_at.hpp \
		  cppa/util/type_list.hpp \
		  cppa/util/type_list_apply.hpp \
		  cppa/util/type_list_pop_back.hpp \
		  cppa/util/void_type.hpp

SOURCES = src/abstract_type_list.cpp \
		  src/actor.cpp \
		  src/actor_behavior.cpp \
		  src/binary_deserializer.cpp \
		  src/binary_serializer.cpp \
		  src/blocking_message_queue.cpp \
		  src/channel.cpp \
		  src/context.cpp \
		  src/converted_thread_context.cpp \
		  src/demangle.cpp \
		  src/deserializer.cpp \
		  src/group.cpp \
		  src/message.cpp \
		  src/mock_scheduler.cpp \
		  src/object.cpp \
		  src/primitive_variant.cpp \
		  src/scheduler.cpp \
		  src/serializer.cpp \
		  src/shared_spinlock.cpp \
		  src/to_uniform_name.cpp \
		  src/uniform_type_info.cpp \
		  src/untyped_tuple.cpp

OBJECTS = $(SOURCES:.cpp=.o)

LIB_NAME = libcppa.dylib

%.o : %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -fPIC -c $< -o $@

$(LIB_NAME) : $(OBJECTS) $(HEADERS)
	$(CXX) $(LIBS) -dynamiclib -o $(LIB_NAME) $(OBJECTS)

all : $(LIB_NAME) $(OBJECTS)

clean:
	rm -f $(LIB_NAME) $(OBJECTS)

