ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libcppa.la

libcppa_la_SOURCES = \
  src/abstract_tuple.cpp \
  src/abstract_type_list.cpp \
  src/actor_behavior.cpp \
  src/actor_count.cpp \
  src/actor.cpp \
  src/actor_proxy_cache.cpp \
  src/actor_proxy.cpp \
  src/actor_registry.cpp \
  src/addressed_message.cpp \
  src/any_tuple.cpp \
  src/any_tuple_iterator.cpp \
  src/atom.cpp \
  src/attachable.cpp \
  src/binary_deserializer.cpp \
  src/binary_serializer.cpp \
  src/blocking_message_queue.cpp \
  src/channel.cpp \
  src/local_actor.cpp \
  src/converted_thread_context.cpp \
  src/cppa.cpp \
  src/delegate.cpp \
  src/demangle.cpp \
  src/deserializer.cpp \
  src/duration.cpp \
  src/empty_tuple.cpp \
  src/exception.cpp \
  src/fiber.cpp \
  src/group.cpp \
  src/group_manager.cpp \
  src/intermediate.cpp \
  src/invokable.cpp \
  src/invoke_rules.cpp \
  src/mailman.cpp \
  src/matcher_arguments.cpp \
  src/message_queue.cpp \
  src/mock_scheduler.cpp \
  src/native_socket.cpp \
  src/network_manager.cpp \
  src/object_array.cpp \
  src/object.cpp \
  src/post_office.cpp \
  src/post_office_msg.cpp \
  src/primitive_variant.cpp \
  src/process_information.cpp \
  src/ripemd_160.cpp \
  src/scheduled_actor.cpp \
  src/scheduler.cpp \
  src/serializer.cpp \
  src/shared_spinlock.cpp \
  src/singleton_manager.cpp \
  src/string_serialization.cpp \
  src/task_scheduler.cpp \
  src/thread_pool_scheduler.cpp \
  src/to_uniform_name.cpp \
  src/unicast_network.cpp \
  src/uniform_type_info.cpp \
  src/yielding_message_queue.cpp \
  src/yield_interface.cpp

if VERSIONED_INCLUDE_DIR
library_includedir = $(includedir)/cppa/$(PACKAGE_VERSION)/
else
library_includedir = $(includedir)/
endif

nobase_library_include_HEADERS = \
  cppa/actor_behavior.hpp \
  cppa/actor.hpp \
  cppa/actor_proxy.hpp \
  cppa/announce.hpp \
  cppa/any_tuple.hpp \
  cppa/any_type.hpp \
  cppa/atom.hpp \
  cppa/attachable.hpp \
  cppa/binary_deserializer.hpp \
  cppa/binary_serializer.hpp \
  cppa/channel.hpp \
  cppa/config.hpp \
  cppa/local_actor.hpp \
  cppa/cow_ptr.hpp \
  cppa/cppa.hpp \
  cppa/deserializer.hpp \
  cppa/detail/abstract_actor.hpp \
  cppa/detail/abstract_message_queue.hpp \
  cppa/detail/abstract_tuple.hpp \
  cppa/detail/actor_count.hpp \
  cppa/detail/actor_proxy_cache.hpp \
  cppa/detail/actor_registry.hpp \
  cppa/detail/addressed_message.hpp \
  cppa/detail/atom_val.hpp \
  cppa/detail/blocking_message_queue.hpp \
  cppa/detail/boxed.hpp \
  cppa/detail/buffer.hpp \
  cppa/detail/channel.hpp \
  cppa/detail/converted_thread_context.hpp \
  cppa/detail/decorated_tuple.hpp \
  cppa/detail/default_uniform_type_info_impl.hpp \
  cppa/detail/delegate.hpp \
  cppa/detail/demangle.hpp \
  cppa/detail/empty_tuple.hpp \
  cppa/detail/get_behavior.hpp \
  cppa/detail/group_manager.hpp \
  cppa/detail/intermediate.hpp \
  cppa/detail/invokable.hpp \
  cppa/detail/list_member.hpp \
  cppa/detail/mailman.hpp \
  cppa/detail/map_member.hpp \
  cppa/detail/matcher_arguments.hpp \
  cppa/detail/matcher.hpp \
  cppa/detail/mock_scheduler.hpp \
  cppa/detail/native_socket.hpp \
  cppa/detail/network_manager.hpp \
  cppa/detail/object_array.hpp \
  cppa/detail/object_impl.hpp \
  cppa/detail/pair_member.hpp \
  cppa/detail/post_office.hpp \
  cppa/detail/post_office_msg.hpp \
  cppa/detail/primitive_member.hpp \
  cppa/detail/ptype_to_type.hpp \
  cppa/detail/receive_loop_helper.hpp \
  cppa/detail/ref_counted_impl.hpp \
  cppa/detail/scheduled_actor.hpp \
  cppa/detail/serialize_tuple.hpp \
  cppa/detail/singleton_manager.hpp \
  cppa/detail/swap_bytes.hpp \
  cppa/detail/task_scheduler.hpp \
  cppa/detail/tdata.hpp \
  cppa/detail/thread_pool_scheduler.hpp \
  cppa/detail/to_uniform_name.hpp \
  cppa/detail/tuple_vals.hpp \
  cppa/detail/type_to_ptype.hpp \
  cppa/detail/unboxed.hpp \
  cppa/detail/uniform_type_info_map.hpp \
  cppa/detail/yielding_message_queue.hpp \
  cppa/detail/yield_interface.hpp \
  cppa/exception.hpp \
  cppa/exit_reason.hpp \
  cppa/from_string.hpp \
  cppa/get.hpp \
  cppa/get_view.hpp \
  cppa/group.hpp \
  cppa/intrusive_ptr.hpp \
  cppa/invoke.hpp \
  cppa/invoke_rules.hpp \
  cppa/match.hpp \
  cppa/message_queue.hpp \
  cppa/object.hpp \
  cppa/on.hpp \
  cppa/primitive_type.hpp \
  cppa/primitive_variant.hpp \
  cppa/process_information.hpp \
  cppa/receive.hpp \
  cppa/ref_counted.hpp \
  cppa/scheduler.hpp \
  cppa/scheduling_hint.hpp \
  cppa/serializer.hpp \
  cppa/to_string.hpp \
  cppa/tuple.hpp \
  cppa/tuple_view.hpp \
  cppa/uniform_type_info.hpp \
  cppa/util/abstract_type_list.hpp \
  cppa/util/abstract_uniform_type_info.hpp \
  cppa/util/a_matches_b.hpp \
  cppa/util/any_tuple_iterator.hpp \
  cppa/util/apply.hpp \
  cppa/util/at.hpp \
  cppa/util/callable_trait.hpp \
  cppa/util/comparable.hpp \
  cppa/util/compare_tuples.hpp \
  cppa/util/concat_type_lists.hpp \
  cppa/util/conjunction.hpp \
  cppa/util/disable_if.hpp \
  cppa/util/disjunction.hpp \
  cppa/util/duration.hpp \
  cppa/util/element_at.hpp \
  cppa/util/enable_if.hpp \
  cppa/util/eval_first_n.hpp \
  cppa/util/eval_type_list.hpp \
  cppa/util/eval_type_lists.hpp \
  cppa/util/fiber.hpp \
  cppa/util/filter_type_list.hpp \
  cppa/util/first_n.hpp \
  cppa/util/has_copy_member_fun.hpp \
  cppa/util.hpp \
  cppa/util/if_else.hpp \
  cppa/util/is_comparable.hpp \
  cppa/util/is_copyable.hpp \
  cppa/util/is_forward_iterator.hpp \
  cppa/util/is_iterable.hpp \
  cppa/util/is_legal_tuple_type.hpp \
  cppa/util/is_one_of.hpp \
  cppa/util/is_primitive.hpp \
  cppa/util/pop_back.hpp \
  cppa/util/pt_dispatch.hpp \
  cppa/util/pt_token.hpp \
  cppa/util/remove_const_reference.hpp \
  cppa/util/replace_type.hpp \
  cppa/util/reverse_type_list.hpp \
  cppa/util/ripemd_160.hpp \
  cppa/util/rm_ref.hpp \
  cppa/util/shared_lock_guard.hpp \
  cppa/util/shared_spinlock.hpp \
  cppa/util/single_reader_queue.hpp \
  cppa/util/singly_linked_list.hpp \
  cppa/util/type_list_apply.hpp \
  cppa/util/type_list.hpp \
  cppa/util/upgrade_lock_guard.hpp \
  cppa/util/void_type.hpp \
  cppa/util/wrapped.hpp

libcppa_la_CXXFLAGS = --std=c++0x -pedantic -Wall -Wextra
libcppa_la_LDFLAGS = -release $(PACKAGE_VERSION) $(BOOST_CPPFLAGS)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libcppa.pc

SUBDIRS = . unit_testing
