cmake_minimum_required(VERSION 2.8)
project(cppa CXX C)

set(LIBCPPA_VERSION_MAJOR 0)
set(LIBCPPA_VERSION_MINOR 5)
set(LIBCPPA_VERSION_PATCH 4)

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
  set(CMAKE_CXX_FLAGS_DEBUG          "")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "")
  set(CMAKE_CXX_FLAGS_RELEASE        "")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "")
else (CMAKE_CXX_FLAGS)
  set(CXXFLAGS_PROVIDED false)
  set(CMAKE_CXX_FLAGS                "-std=c++11 -Wextra -Wall -pedantic")
  set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os")
  set(CMAKE_CXX_FLAGS_RELEASE        "-O4")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")
endif (CMAKE_CXX_FLAGS)

include(CheckIncludeFiles)
check_include_files("valgrind/valgrind.h" HAVE_VALGRIND_H)
set(VALGRIND "no")
if (NOT HAVE_VALGRIND_H)
  add_definitions(-DNVALGRIND)
else (NOT HAVE_VALGRIND_H)
  set(VALGRIND "yes")
endif (NOT HAVE_VALGRIND_H)

# check for g++ >= 4.7 or clang++ > = 3.2
try_run(ProgramResult
        CompilationSucceeded
        ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/get_compiler_version.cpp
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
    message(STATUS "NOTE: Automatically added -stdlib=libc++ flag, "
                   "you can override this by defining CMAKE_CXX_FLAGS "
                   "(see 'configure --help')")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
  endif ()
else ()
  message(FATAL_ERROR "Your C++ compiler does not support C++11 "
                      "or is not supported")
endif ()

# set build type (evaluate ENABLE_DEBUG flag)
if (ENABLE_DEBUG)
  set(CMAKE_BUILD_TYPE Debug)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCPPA_DEBUG")
endif (ENABLE_DEBUG)

if (CPPA_LOG_LEVEL)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCPPA_LOG_LEVEL=${CPPA_LOG_LEVEL}")
endif(CPPA_LOG_LEVEL)

# set build default build type if not set
if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif ("${CMAKE_BUILD_TYPE}" STREQUAL "")

set(LIBCPPA_SRC
    src/abstract_scheduled_actor.cpp
    src/abstract_tuple.cpp
    src/actor.cpp
    src/actor_addressing.cpp
    src/actor_count.cpp
    src/actor_proxy.cpp
    src/actor_registry.cpp
    src/any_tuple.cpp
    src/atom.cpp
    src/attachable.cpp
    src/behavior_stack.cpp
    src/binary_deserializer.cpp
    src/binary_serializer.cpp
    src/buffer.cpp
    src/channel.cpp
    src/context_switching_actor.cpp
    src/continuable_reader.cpp
    src/continuable_writer.cpp
    src/decorated_names_map.cpp
    src/default_actor_addressing.cpp
    src/default_actor_proxy.cpp
    src/default_peer.cpp
    src/default_peer_acceptor.cpp
    src/default_protocol.cpp
    src/demangle.cpp
    src/deserializer.cpp
    src/duration.cpp
    src/empty_tuple.cpp
    src/event_based_actor.cpp
    src/exception.cpp
    src/factory.cpp
    src/fd_util.cpp
    src/fiber.cpp
    src/group.cpp
    src/group_manager.cpp
    src/ipv4_acceptor.cpp
    src/ipv4_io_stream.cpp
    src/local_actor.cpp
    src/logging.cpp
    src/match.cpp
    src/memory.cpp
    src/memory_managed.cpp
    src/message_header.cpp
    src/middleman.cpp
    src/object.cpp
    src/object_array.cpp
    src/opt.cpp
    src/partial_function.cpp
    src/primitive_variant.cpp
    src/process_information.cpp
    src/protocol.cpp
    src/receive.cpp
    src/recursive_queue_node.cpp
    src/ref_counted.cpp
    src/response_handle.cpp
    src/ripemd_160.cpp
    src/scheduled_actor.cpp
    src/scheduled_actor_dummy.cpp
    src/scheduler.cpp
    src/self.cpp
    src/serializer.cpp
    src/shared_spinlock.cpp
    src/singleton_manager.cpp
    src/string_serialization.cpp
    src/thread_mapped_actor.cpp
    src/thread_pool_scheduler.cpp
    src/to_uniform_name.cpp
    src/unicast_network.cpp
    src/uniform_type_info.cpp
    src/weak_ptr_anchor.cpp
    src/yield_interface.cpp)

if (BOOST_ROOT)
  # Prevent falling back to system paths when using a custom Boost prefix.
  set(Boost_NO_SYSTEM_PATHS true)
endif ()

find_package(PTHREAD REQUIRED)

set(INCLUDE_DIRS . ${PTHREAD_INCLUDE_DIR})
set(LD_DIRS)
set(LD_FLAGS ${CMAKE_LD_LIBS} ${PTHREAD_LIBRARIES})

if (DISABLE_CONTEXT_SWITCHING)
  # explicitly disabled
else (DISABLE_CONTEXT_SWITCHING)
  find_package(Boost 1.51.0 COMPONENTS context)
  # without REQUIRED, find_package(Boost...) returns with Boost_FOUND=FALSE
  if (NOT Boost_FOUND)
    set(DISABLE_CONTEXT_SWITCHING true)
  else (NOT Boost_FOUND)
    # This hack fixes a problem when the linker search path is the same as
    # the one provided by the compiler. In this case, CMake replaces the absolute
    # path (e.g., /path/to/lib.so) with -l<lib>, which may cause it to pick up the wrong
    # library. So when this replacement happens, we ensure that the right
    # library gets picked by adding a -L directive for the affected libraries
    # (which is just Boost Context here).
    set(CMAKE_EXE_LINKER_FLAGS -L${Boost_LIBRARY_DIRS})
    set(INCLUDE_DIRS ${INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})
    set(LD_DIRS ${LD_DIRS} ${Boost_LIBRARY_DIRS})
    set(LD_FLAGS ${LD_FLAGS} ${Boost_CONTEXT_LIBRARY})
    set(DISABLE_CONTEXT_SWITCHING false)
  endif (NOT Boost_FOUND)
endif (DISABLE_CONTEXT_SWITCHING)

# set compiler flag when compiling w/o context-switching actors
if (DISABLE_CONTEXT_SWITCHING)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCPPA_DISABLE_CONTEXT_SWITCHING")
endif ()

# bulid shared library only if not comiling static only
if (NOT "${CPPA_BUILD_STATIC_ONLY}" STREQUAL "yes")
  add_library(libcppa SHARED ${LIBCPPA_SRC})
  target_link_libraries(libcppa ${LD_FLAGS})
  set(LIBRARY_VERSION ${LIBCPPA_VERSION_MAJOR}.${LIBCPPA_VERSION_MINOR}.${LIBCPPA_VERSION_PATCH})
  set(LIBRARY_SOVERSION ${LIBCPPA_VERSION_MAJOR})
  set_target_properties(libcppa 
                        PROPERTIES
                        SOVERSION ${LIBRARY_SOVERSION}
                        VERSION ${LIBRARY_VERSION}
                        OUTPUT_NAME cppa)
  install(TARGETS libcppa LIBRARY DESTINATION lib)
endif ()

# build static library if --build-static or --build-static-only was set
if ("${CPPA_BUILD_STATIC_ONLY}" STREQUAL "yes" OR "${CPPA_BUILD_STATIC}" STREQUAL "yes")
  add_library(libcppaStatic STATIC ${LIBCPPA_SRC})
  set_target_properties(libcppaStatic PROPERTIES OUTPUT_NAME cppa_static)
  install(TARGETS libcppaStatic ARCHIVE DESTINATION lib)
endif ()

link_directories(${LD_DIRS})
include_directories(${INCLUDE_DIRS})

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

if ("${CPPA_BUILD_STATIC_ONLY}" STREQUAL "yes")
  set (CPPA_LIBRARY ${LIBRARY_OUTPUT_PATH}/libcppa_static.a ${LD_FLAGS})
else ()
  if (APPLE)
    set (CPPA_LIBRARY ${LIBRARY_OUTPUT_PATH}/libcppa.dylib)
  elseif (UNIX)
    set (CPPA_LIBRARY ${LIBRARY_OUTPUT_PATH}/libcppa.so)
  else ()
    message (SEND_FATAL "Host platform not supported ...")
  endif ()
endif ()

if (EXECUTABLE_OUTPUT_PATH)
else ()
  set (EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin CACHE PATH "Single directory for all executables")
endif ()

# set up subdirectories
if (NOT "${CPPA_NO_UNIT_TESTS}" STREQUAL "yes")
  enable_testing()
  add_subdirectory(unit_testing)
  if ("${CPPA_BUILD_STATIC_ONLY}" STREQUAL "yes")
    add_dependencies(all_unit_tests libcppaStatic)
  else ()
    add_dependencies(all_unit_tests libcppa)
  endif ()
endif ()
if (NOT "${CPPA_NO_EXAMPLES}" STREQUAL "yes")
  add_subdirectory(examples)
  if ("${CPPA_BUILD_STATIC_ONLY}" STREQUAL "yes")
    add_dependencies(all_examples libcppaStatic)
  else ()
    add_dependencies(all_examples libcppa)
  endif ()
endif ()

# set optional flags
string(TOUPPER ${CMAKE_BUILD_TYPE} build_type)
set(CONTEXT_SWITCHING "yes")
if (DISABLE_CONTEXT_SWITCHING)
    set(CONTEXT_SWITCHING "no")
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
endif (DOXYGEN_FOUND)

# set variables for summary
set(LOG_LEVEL_STR "none")
if (CPPA_LOG_LEVEL)
  if (${CPPA_LOG_LEVEL} EQUAL 0)
    set(LOG_LEVEL_STR "ERROR")
  elseif (${CPPA_LOG_LEVEL} EQUAL 1)
    set(LOG_LEVEL_STR "WARNING")
  elseif (${CPPA_LOG_LEVEL} EQUAL 2)
    set(LOG_LEVEL_STR "INFO")
  elseif (${CPPA_LOG_LEVEL} EQUAL 3)
    set(LOG_LEVEL_STR "DEBUG")
  elseif (${CPPA_LOG_LEVEL} EQUAL 4)
    set(LOG_LEVEL_STR "TRACE")
  else ()
    set(LOG_LEVEL_STR "invalid")
  endif ()
endif (CPPA_LOG_LEVEL)

macro (toYesNo in out)
  if (${in})
    set(${out} "yes")
  else ()
    set(${out} "no")
  endif ()
endmacro ()

macro (invertYesNo in out)
  if ("${in}" STREQUAL "yes")
    set(${out} "no")
  else ()
    set(${out} "yes")
  endif ()
endmacro ()

toYesNo(ENABLE_DEBUG DEBUG_MODE_STR)
invertYesNo(CPPA_NO_EXAMPLES BUILD_EXAMPLES)
invertYesNo(CPPA_NO_UNIT_TESTS BUILD_UNIT_TESTS)

if (NOT "${CPPA_BUILD_STATIC}" STREQUAL "yes")
  set(CPPA_BUILD_STATIC "no")
endif ()

if (NOT "${CPPA_BUILD_STATIC_ONLY}" STREQUAL "yes")
  set(CPPA_BUILD_STATIC_ONLY "no")
else ()
  set(CPPA_BUILD_STATIC "yes")
endif ()

# done (print summary)
message("\n====================|  Build Summary  |===================="
        "\n"
        "\nLibcppa version:   ${LIBRARY_VERSION}"
        "\n"
        "\nBuild type:        ${CMAKE_BUILD_TYPE}"
        "\nDebug mode:        ${DEBUG_MODE_STR}"
        "\nLog level:         ${LOG_LEVEL_STR}"
        "\nContext switching: ${CONTEXT_SWITCHING}"
        "\nValgrind:          ${VALGRIND}"
        "\nBuild examples:    ${BUILD_EXAMPLES}"
        "\nBuild unit tests:  ${BUILD_UNIT_TESTS}"
        "\nBuild static:      ${CPPA_BUILD_STATIC}"
        "\nBulid static only: ${CPPA_BUILD_STATIC_ONLY}"
        "\n"
        "\nCXX:               ${CMAKE_CXX_COMPILER}"
        "\nCXXFLAGS:          ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${build_type}}"
        "\nLD_DIRS:           ${LD_DIRS}"
        "\nLIBRARIES:         ${LD_FLAGS}"
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
