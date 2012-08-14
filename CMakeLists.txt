cmake_minimum_required(VERSION 2.4)
project(cppa CXX)

set(LIBCPPA_VERSION_MAJOR 0)
set(LIBCPPA_VERSION_MINOR 3)
set(LIBCPPA_VERSION_PATCH 3)

# prohibit in-source builds
if ("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(FATAL_ERROR "In-source builds are not allowed. Please use "
                        "./configure to choose a build directory and "
                        "initialize the build configuration.")
endif ()

set (CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# check if the user provided CXXFLAGS on the command line
if (CMAKE_CXX_FLAGS)
    set(CXXFLAGS_PROVIDED true)
endif ()
if (CXXFLAGS_PROVIDED)
  set(CMAKE_CXX_FLAGS_DEBUG          "")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "")
  set(CMAKE_CXX_FLAGS_RELEASE        "")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "")
else ()
  set(CMAKE_CXX_FLAGS                "-std=c++11 -Wextra -Wall -pedantic")
  set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os")
  set(CMAKE_CXX_FLAGS_RELEASE        "-O4")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")
endif ()

# check for g++ >= 4.7 or clang++ > = 3.2
try_run(ProgramResult
        CompilationSucceeded
        ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/src/get_compiler_version.cpp
        RUN_OUTPUT_VARIABLE CompilerVersion)
if (NOT CompilationSucceeded OR NOT ProgramResult EQUAL 0)
  message(FATAL_ERROR "Cannot determine compiler version")
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
  if (CompilerVersion VERSION_GREATER 4.6)
    message(STATUS "Found g++ version ${CompilerVersion}")
  else ()
    message(FATAL_ERROR "g++ >= 4.7 required (found: ${CompilerVersion}.")
  endif ()
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  if (CompilerVersion VERSION_GREATER 3.1)
    message(STATUS "Found clang++ version ${CompilerVersion}")
  else ()
    message(FATAL_ERROR "clang++ >= 3.2 required (found: ${CompilerVersion}.")
  endif ()
  if (NOT CXXFLAGS_PROVIDED)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
  endif ()
else ()
  message(FATAL_ERROR "Your C++ compiler does not support C++11.")
endif ()

# set build type (evaluate ENABLE_DEBUG flag)
if (ENABLE_DEBUG)
  set(CMAKE_BUILD_TYPE Debug)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCPPA_DEBUG")
else ()
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif ()

if (DISABLE_CONTEXT_SWITCHING)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCPPA_DISABLE_CONTEXT_SWITCHING")
endif ()

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
    src/buffer.cpp
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
    src/ipv4_acceptor.cpp
    src/ipv4_io_stream.cpp
    src/local_actor.cpp
    src/mailman.cpp
    src/middleman.cpp
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

set(boost_context third_party/boost_context)

# add third party Boost.Context sources if context-switching is enabled
if (NOT DISABLE_CONTEXT_SWITCHING)
  if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    if (APPLE)
      set(fcontext_asm ${boost_context}/src/asm/fcontext_i386_sysv_macho_gas.S)
    else ()
      set(fcontext_asm ${boost_context}/src/asm/fcontext_i386_sysv_elf_gas.S)
    endif ()
  elseif (CMAKE_SIZEOF_VOID_P EQUAL 8)
    if (APPLE)
      set(fcontext_asm ${boost_context}/src/asm/fcontext_x86_64_sysv_macho_gas.S)
    else ()
      set(fcontext_asm ${boost_context}/src/asm/fcontext_x86_64_sysv_elf_gas.S)
    endif ()
  else ()
    message(FATAL_ERROR "Unsupported platform (neither 32 nor 64 bit)")
  endif ()
  set_property(SOURCE ${fcontext_asm} PROPERTY LANGUAGE CXX)
  set(LIBCPPA_SRC
      ${LIBCPPA_SRC}
      ${fcontext_asm}
      ${boost_context}/src/stack_utils_posix.cpp
      ${boost_context}/src/stack_allocator_posix.cpp
      ${boost_context}/src/fcontext.cpp
  )
endif ()

find_package(Boost COMPONENTS thread REQUIRED)

link_directories(${Boost_LIBRARY_DIRS})
include_directories(. ${Boost_INCLUDE_DIRS} ${boost_context}/include)

add_library(libcppa SHARED ${LIBCPPA_SRC})

target_link_libraries(libcppa ${CMAKE_LD_LIBS} ${Boost_THREAD_LIBRARY})

set(LIBRARY_VERSION ${LIBCPPA_VERSION_MAJOR}.${LIBCPPA_VERSION_MINOR}.${LIBCPPA_VERSION_PATCH})
set(LIBRARY_SOVERSION ${LIBCPPA_VERSION_MAJOR})

set_target_properties(libcppa 
                      PROPERTIES
                      SOVERSION ${LIBRARY_SOVERSION}
                      VERSION ${LIBRARY_VERSION}
                      OUTPUT_NAME cppa)

# install shared library
install(TARGETS libcppa LIBRARY DESTINATION lib)

# install includes
install(DIRECTORY cppa/ DESTINATION include/cppa FILES_MATCHING PATTERN "*.hpp")

# process cmake_uninstall.cmake.in
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
               "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
               IMMEDIATE @ONLY)

# add uninstall target
add_custom_target(uninstall
                  COMMAND ${CMAKE_COMMAND} -P
                  ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

if (LIBRARY_OUTPUT_PATH)
  set (CPPA_LIBRARY_OUTPUT_PATH ${LIBRARY_OUTPUT_PATH})
  set (CPPA_LIBRARY_PATH ${LIBRARY_OUTPUT_PATH})
else ()
  set (CPPA_LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/lib)
  set (CPPA_LIBRARY_PATH ${CPPA_LIBRARY_OUTPUT_PATH})
  set (LIBRARY_OUTPUT_PATH ${CPPA_LIBRARY_OUTPUT_PATH} CACHE PATH "Single directory for all libraries")
endif ()

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

enable_testing()

# set up subdirectories
add_subdirectory(unit_testing)
add_subdirectory(examples)
add_subdirectory(benchmarks)

# add libcppa as a dependency to all binaries
add_dependencies(announce_example_1 libcppa)
add_dependencies(announce_example_2 libcppa)
add_dependencies(announce_example_3 libcppa)
add_dependencies(announce_example_4 libcppa)
add_dependencies(announce_example_5 libcppa)
add_dependencies(dancing_kirby libcppa)
add_dependencies(dining_philosophers libcppa)
add_dependencies(hello_world_example libcppa)
add_dependencies(math_actor_example libcppa)
add_dependencies(actor_creation libcppa)
add_dependencies(mailbox_performance libcppa)
add_dependencies(mixed_case libcppa)
add_dependencies(distributed libcppa)
add_dependencies(matching libcppa)
add_dependencies(test__atom libcppa)
add_dependencies(test__intrusive_containers libcppa)
add_dependencies(test__local_group libcppa)
add_dependencies(test__pattern libcppa)
add_dependencies(test__remote_actor libcppa)
add_dependencies(test__serialization libcppa)
add_dependencies(test__uniform_type libcppa)
add_dependencies(test__fixed_vector libcppa)
add_dependencies(test__intrusive_ptr libcppa)
add_dependencies(test__match libcppa)
add_dependencies(test__primitive_variant libcppa)
add_dependencies(test__ripemd_160 libcppa)
add_dependencies(test__spawn libcppa)
add_dependencies(test__type_list libcppa)
add_dependencies(test__yield_interface libcppa)
add_dependencies(test__sync_send libcppa)

# set optional flags
string(TOUPPER ${CMAKE_BUILD_TYPE} build_type)
set(CONTEXT_SWITCHING true)
if (DISABLE_CONTEXT_SWITCHING)
    set(CONTEXT_SWITCHING false)
endif ()

# check for doxygen and add custom "doc" target to Makefile
find_package(Doxygen)
if (DOXYGEN_FOUND)
  configure_file(${CMAKE_SOURCE_DIR}/Doxyfile.in ${CMAKE_SOURCE_DIR}/Doxyfile
                 @ONLY)
  add_custom_target(doc ${DOXYGEN_EXECUTABLE} ${CMAKE_HOME_DIRECTORY}/Doxyfile
                    WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}
                    COMMENT "Generating API documentation with Doxygen"
                    VERBATIM)
endif ()

# done (print summary)
message("\n====================|  Build Summary  |===================="
        "\n"
        "\nLibcppa version:   ${LIBRARY_VERSION}"
        "\n"
        "\nBuild type:        ${CMAKE_BUILD_TYPE}"
        "\nDebug mode:        ${ENABLE_DEBUG}"
        "\nContext switching: ${CONTEXT_SWITCHING}"
        "\n"
        "\nCXX:               ${CMAKE_CXX_COMPILER}"
        "\nCXXFLAGS:          ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${build_type}}"
        "\n"
        "\nSource directory:  ${CMAKE_SOURCE_DIR}"
        "\nBuild directory:   ${CMAKE_BINARY_DIR}"
        "\nExecutable path:   ${EXECUTABLE_OUTPUT_PATH}"
        "\nLibrary path:      ${LIBRARY_OUTPUT_PATH}"
        "\nInstall prefix:    ${CMAKE_INSTALL_PREFIX}"
        "\n"
        "\nBoost:             ${Boost_INCLUDE_DIR}"
        "\n"
        "\n===========================================================\n")
