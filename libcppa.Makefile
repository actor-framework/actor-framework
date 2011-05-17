include Makefile.rules
INCLUDES = -I./

HEADERS = \
          cppa/actor.hpp \
          cppa/actor_behavior.hpp \
          cppa/announce.hpp \
          cppa/any_tuple.hpp \
          cppa/any_type.hpp \
          cppa/binary_deserializer.hpp \
          cppa/binary_serializer.hpp \
          cppa/channel.hpp \
          cppa/config.hpp \
          cppa/context.hpp \
          cppa/cow_ptr.hpp \
          cppa/cppa.hpp \
          cppa/deserializer.hpp \
          cppa/exit_signal.hpp \
          cppa/get.hpp \
          cppa/get_view.hpp \
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
          cppa/util.hpp \
          cppa/detail/abstract_tuple.hpp \
          cppa/detail/blocking_message_queue.hpp \
          cppa/detail/channel.hpp \
          cppa/detail/converted_thread_context.hpp \
          cppa/detail/cpp0x_thread_wrapper.hpp \
          cppa/detail/decorated_tuple.hpp \
          cppa/detail/default_uniform_type_info_impl.hpp \
          cppa/detail/demangle.hpp \
          cppa/detail/intermediate.hpp \
          cppa/detail/invokable.hpp \
          cppa/detail/list_member.hpp \
          cppa/detail/map_member.hpp \
          cppa/detail/matcher.hpp \
          cppa/detail/mock_scheduler.hpp \
          cppa/detail/object_array.hpp \
          cppa/detail/object_impl.hpp \
          cppa/detail/pair_member.hpp \
          cppa/detail/primitive_member.hpp \
          cppa/detail/ptype_to_type.hpp \
          cppa/detail/ref_counted_impl.hpp \
          cppa/detail/serialize_tuple.hpp \
          cppa/detail/swap_bytes.hpp \
          cppa/detail/tdata.hpp \
          cppa/detail/to_uniform_name.hpp \
          cppa/detail/tuple_vals.hpp \
          cppa/detail/type_to_ptype.hpp \
          cppa/util/a_matches_b.hpp \
          cppa/util/abstract_type_list.hpp \
          cppa/util/abstract_uniform_type_info.hpp \
          cppa/util/apply.hpp \
          cppa/util/callable_trait.hpp \
          cppa/util/comparable.hpp \
          cppa/util/compare_tuples.hpp \
          cppa/util/concat_type_lists.hpp \
          cppa/util/conjunction.hpp \
          cppa/util/detach.hpp \
          cppa/util/disable_if.hpp \
          cppa/util/disjunction.hpp \
          cppa/util/enable_if.hpp \
          cppa/util/eval_first_n.hpp \
          cppa/util/eval_type_list.hpp \
          cppa/util/eval_type_lists.hpp \
          cppa/util/filter_type_list.hpp \
          cppa/util/first_n.hpp \
          cppa/util/has_copy_member_fun.hpp \
          cppa/util/if_else_type.hpp \
          cppa/util/is_comparable.hpp \
          cppa/util/is_copyable.hpp \
          cppa/util/is_forward_iterator.hpp \
          cppa/util/is_iterable.hpp \
          cppa/util/is_legal_tuple_type.hpp \
          cppa/util/is_one_of.hpp \
          cppa/util/is_primitive.hpp \
          cppa/util/pt_token.hpp \
          cppa/util/remove_const_reference.hpp \
          cppa/util/replace_type.hpp \
          cppa/util/reverse_type_list.hpp \
          cppa/util/rm_ref.hpp \
          cppa/util/shared_lock_guard.hpp \
          cppa/util/shared_spinlock.hpp \
          cppa/util/single_reader_queue.hpp \
          cppa/util/singly_linked_list.hpp \
          cppa/util/type_at.hpp \
          cppa/util/type_list.hpp \
          cppa/util/type_list_apply.hpp \
          cppa/util/type_list_pop_back.hpp \
          cppa/util/upgrade_lock_guard.hpp \
          cppa/util/void_type.hpp \
          cppa/util/wrapped_type.hpp

SOURCES = \
          src/abstract_type_list.cpp \
          src/actor.cpp \
          src/actor_behavior.cpp \
          src/any_tuple.cpp \
          src/binary_deserializer.cpp \
          src/binary_serializer.cpp \
          src/blocking_message_queue.cpp \
          src/channel.cpp \
          src/context.cpp \
          src/converted_thread_context.cpp \
          src/demangle.cpp \
          src/deserializer.cpp \
          src/exit_signal.cpp \
          src/group.cpp \
          src/message.cpp \
          src/mock_scheduler.cpp \
          src/object.cpp \
          src/object_array.cpp \
          src/primitive_variant.cpp \
          src/scheduler.cpp \
          src/serializer.cpp \
          src/shared_spinlock.cpp \
          src/to_uniform_name.cpp \
          src/uniform_type_info.cpp

OBJECTS = $(SOURCES:.cpp=.o)

LIB_NAME = libcppa.dylib

%.o : %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -fPIC -c $< -o $@

$(LIB_NAME) : $(OBJECTS) $(HEADERS)
	$(CXX) $(LIBS) -dynamiclib -o $(LIB_NAME) $(OBJECTS)

all : $(LIB_NAME) $(OBJECTS)

clean:
	rm -f $(LIB_NAME) $(OBJECTS)
