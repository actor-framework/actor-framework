ACLOCAL_AMFLAGS = -I ../m4

bin_PROGRAMS = cppatest

cppatest_SOURCES = \
  ../src/abstract_tuple.cpp \
  ../src/abstract_type_list.cpp \
  ../src/actor_behavior.cpp \
  ../src/actor_count.cpp \
  ../src/actor.cpp \
  ../src/actor_proxy_cache.cpp \
  ../src/actor_proxy.cpp \
  ../src/actor_registry.cpp \
  ../src/addressed_message.cpp \
  ../src/any_tuple.cpp \
  ../src/any_tuple_iterator.cpp \
  ../src/atom.cpp \
  ../src/attachable.cpp \
  ../src/binary_deserializer.cpp \
  ../src/binary_serializer.cpp \
  ../src/blocking_message_queue.cpp \
  ../src/channel.cpp \
  ../src/local_actor.cpp \
  ../src/converted_thread_context.cpp \
  ../src/cppa.cpp \
  ../src/delegate.cpp \
  ../src/demangle.cpp \
  ../src/deserializer.cpp \
  ../src/duration.cpp \
  ../src/empty_tuple.cpp \
  ../src/exception.cpp \
  ../src/fiber.cpp \
  ../src/group.cpp \
  ../src/group_manager.cpp \
  ../src/intermediate.cpp \
  ../src/invokable.cpp \
  ../src/invoke_rules.cpp \
  ../src/mailman.cpp \
  ../src/matcher_arguments.cpp \
  ../src/message_queue.cpp \
  ../src/mock_scheduler.cpp \
  ../src/native_socket.cpp \
  ../src/network_manager.cpp \
  ../src/object_array.cpp \
  ../src/object.cpp \
  ../src/post_office.cpp \
  ../src/post_office_msg.cpp \
  ../src/primitive_variant.cpp \
  ../src/process_information.cpp \
  ../src/ripemd_160.cpp \
  ../src/scheduled_actor.cpp \
  ../src/scheduler.cpp \
  ../src/serializer.cpp \
  ../src/shared_spinlock.cpp \
  ../src/singleton_manager.cpp \
  ../src/string_serialization.cpp \
  ../src/task_scheduler.cpp \
  ../src/thread_pool_scheduler.cpp \
  ../src/to_uniform_name.cpp \
  ../src/unicast_network.cpp \
  ../src/uniform_type_info.cpp \
  ../src/yielding_message_queue.cpp \
  ../src/yield_interface.cpp \
  ../unit_testing/hash_of.cpp \
  ../unit_testing/main.cpp \
  ../unit_testing/ping_pong.cpp \
  ../unit_testing/test__a_matches_b.cpp \
  ../unit_testing/test__atom.cpp \
  ../unit_testing/test__intrusive_ptr.cpp \
  ../unit_testing/test__local_group.cpp \
  ../unit_testing/test__primitive_variant.cpp \
  ../unit_testing/test__queue_performance.cpp \
  ../unit_testing/test__remote_actor.cpp \
  ../unit_testing/test__ripemd_160.cpp \
  ../unit_testing/test__serialization.cpp \
  ../unit_testing/test__spawn.cpp \
  ../unit_testing/test__tuple.cpp \
  ../unit_testing/test__type_list.cpp \
  ../unit_testing/test__uniform_type.cpp

AM_CPPFLAGS = -I../
cppatest_CXXFLAGS = --std=c++0x -pedantic -Wall -Wextra
AM_LDFLAGS = $(BOOST_LDFLAGS) $(BOOST_THREAD_LIB)

