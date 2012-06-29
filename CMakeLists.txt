cmake_minimum_required(VERSION 2.4)
project(cppa CXX)

set (CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wextra -Wall -pedantic") 

set(LIBCPPA_SRC
    src/abstract_tuple.cpp
    src/actor.cpp
    src/actor_count.cpp
    src/actor_proxy.cpp
    src/actor_proxy_cache.cpp
    src/actor_registry.cpp
    src/addressed_message.cpp
    src/any_tuple.cpp
    src/atom.cpp
    src/attachable.cpp
    src/behavior_stack.cpp
    src/binary_deserializer.cpp
    src/binary_serializer.cpp
    src/channel.cpp
    src/thread_mapped_actor.cpp
    src/demangle.cpp
    src/deserializer.cpp
    src/duration.cpp
    src/empty_tuple.cpp
    src/event_based_actor.cpp
    src/exception.cpp
    src/factory.cpp
    src/fiber.cpp
    src/group.cpp
    src/group_manager.cpp
    src/local_actor.cpp
    src/mailman.cpp
    src/native_socket.cpp
    src/network_manager.cpp
    src/object.cpp
    src/object_array.cpp
    src/partial_function.cpp
    src/pattern.cpp
    src/post_office.cpp
    src/primitive_variant.cpp
    src/process_information.cpp
    src/receive.cpp
    src/ripemd_160.cpp
    src/scheduled_actor.cpp
    src/scheduled_actor_dummy.cpp
    src/scheduler.cpp
    src/self.cpp
    src/serializer.cpp
    src/shared_spinlock.cpp
    src/singleton_manager.cpp
    src/string_serialization.cpp
    src/thread_pool_scheduler.cpp
    src/to_uniform_name.cpp
    src/unicast_network.cpp
    src/uniform_type_info.cpp
    src/yield_interface.cpp
    src/context_switching_actor.cpp
)

set(boost_context third_party/boost_context/)

# add third_party boost_context sources
if(APPLE)
  set_property(SOURCE
               ${boost_context}/src/asm/fcontext_x86_64_sysv_macho_gas.S
               PROPERTY LANGUAGE CXX)
  set(LIBCPPA_SRC
      ${LIBCPPA_SRC}
      ${boost_context}/src/stack_utils_posix.cpp
      ${boost_context}/src/stack_allocator_posix.cpp
      ${boost_context}/src/fcontext.cpp
      ${boost_context}/src/asm/fcontext_x86_64_sysv_macho_gas.S
  )
elseif(UNIX)
  if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set_property(SOURCE
                 ${boost_context}/src/asm/fcontext_i386_sysv_elf_gas.S
                 PROPERTY LANGUAGE CXX)
    set(fcontext_asm ${boost_context}/src/asm/fcontext_i386_sysv_elf_gas.S)
  else()
    set_property(SOURCE
                 ${boost_context}/src/asm/fcontext_x86_64_sysv_elf_gas.S
                 PROPERTY LANGUAGE CXX)
    set(fcontext_asm ${boost_context}/src/asm/fcontext_x86_64_sysv_elf_gas.S)
  endif()
  set(LIBCPPA_SRC
      ${LIBCPPA_SRC}
      ${fcontext_asm}
      ${boost_context}/src/stack_utils_posix.cpp
      ${boost_context}/src/stack_allocator_posix.cpp
      ${boost_context}/src/fcontext.cpp
  )
endif()

find_package(Boost COMPONENTS thread REQUIRED)

link_directories(${Boost_LIBRARY_DIRS})
include_directories(. ${Boost_INCLUDE_DIRS} ${boost_context}/include)

add_library(libcppa SHARED ${LIBCPPA_SRC})

target_link_libraries(libcppa ${CMAKE_LD_LIBS} ${Boost_THREAD_LIBRARY})

set(LIBCPPA_VERSION_MAJOR 0)
set(LIBCPPA_VERSION_MINOR 2)
set(LIBCPPA_VERSION_PATCH 0)
set(LIBRARY_VERSION ${LIBCPPA_VERSION_MAJOR}.${LIBCPPA_VERSION_MINOR}.${LIBCPPA_VERSION_PATCH})
set(LIBRARY_SOVERSION ${LIBCPPA_VERSION_MAJOR})

set_target_properties(libcppa PROPERTIES SOVERSION ${LIBRARY_SOVERSION} VERSION ${LIBRARY_VERSION} OUTPUT_NAME cppa)
install(TARGETS libcppa LIBRARY DESTINATION lib)

# install includes
install(DIRECTORY cppa/ DESTINATION include/cppa
    FILES_MATCHING PATTERN "*.hpp")
# uninstall target
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P
    ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)



if (LIBRARY_OUTPUT_PATH)
  set (CPPA_LIBRARY_OUTPUT_PATH ${LIBRARY_OUTPUT_PATH})
  set (CPPA_LIBRARY_PATH ${LIBRARY_OUTPUT_PATH})
else()
  set (CPPA_LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/lib)
  set (CPPA_LIBRARY_PATH ${CPPA_LIBRARY_OUTPUT_PATH})
  set (LIBRARY_OUTPUT_PATH ${CPPA_LIBRARY_OUTPUT_PATH} CACHE PATH "Single directory for all libraries")
endif()

# setting path to cppa headers and libcppa
set (CPPA_INCLUDE_PATH ${CMAKE_SOURCE_DIR}/libcppa)
set (CPPA_INCLUDE ${CPPA_INCLUDE_PATH})

if (APPLE)
  set (CPPA_LIBRARY ${LIBRARY_OUTPUT_PATH}/libcppa.dylib)
elseif (UNIX)
  set (CPPA_LIBRARY ${LIBRARY_OUTPUT_PATH}/libcppa.so)
else ()
  message (SEND_FATAL "Host platform not supported ...")
endif ()

if (EXECUTABLE_OUTPUT_PATH)
else ()
  set (EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin CACHE PATH "Single directory for all executables")
endif ()

add_subdirectory(unit_testing)
add_subdirectory(examples)
add_subdirectory(benchmarks)
#add_subdirectory(benchmarks)
#add_dependencies(unit_tests libcppa)
#add_dependencies(benchmarks libcppa)
add_dependencies(announce_example_1 libcppa)
add_dependencies(announce_example_2 libcppa)
add_dependencies(announce_example_3 libcppa)
add_dependencies(announce_example_4 libcppa)
add_dependencies(announce_example_5 libcppa)
add_dependencies(dancing_kirby libcppa)
add_dependencies(dining_philosophers libcppa)
add_dependencies(hello_world_example libcppa)
add_dependencies(math_actor_example libcppa)
add_dependencies(unit_tests libcppa)
add_dependencies(actor_creation libcppa)
add_dependencies(mailbox_performance libcppa)
add_dependencies(mixed_case libcppa)
add_dependencies(distributed libcppa)
add_dependencies(matching libcppa)
