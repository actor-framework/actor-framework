cmake_minimum_required(VERSION 2.8)
project(cppa C CXX)

set(LIBCPPA_VERSION_MAJOR 0)
set(LIBCPPA_VERSION_MINOR 9)
set(LIBCPPA_VERSION_PATCH 0)

# prohibit in-source builds
if ("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(FATAL_ERROR "In-source builds are not allowed. Please use "
                        "./configure to choose a build directory and "
                        "initialize the build configuration.")
endif ()

set (CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# set binary output path
if ("${EXECUTABLE_OUTPUT_PATH}" STREQUAL "")
  set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/bin")
endif ()

# set library output path
if ("${LIBRARY_OUTPUT_PATH}" STREQUAL "")
  set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/lib")
endif ()

# set library output path to binary output path for Xcode projects
if ("${CMAKE_GENERATOR}" STREQUAL "Xcode")
  set(LIBRARY_OUTPUT_PATH "${EXECUTABLE_OUTPUT_PATH}")
endif ()

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
  if (MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWIN32")
    include (GenerateExportHeader)
  endif(MINGW)
  set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os")
  set(CMAKE_CXX_FLAGS_RELEASE        "-O3 -DNDEBUG")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")
endif (CMAKE_CXX_FLAGS)

include(CheckIncludeFileCXX)
check_include_file_cxx("valgrind/valgrind.h" HAVE_VALGRIND_H)
if (HAVE_VALGRIND_H)
  set(VALGRIND "yes")
  add_definitions(-DCPPA_ANNOTATE_VALGRIND)
else (HAVE_VALGRIND_H)
  set(VALGRIND "no")
endif (HAVE_VALGRIND_H)

# check for g++ >= 4.7 or clang++ > = 3.2
try_run(ProgramResult
        CompilationSucceeded
  ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/get_compiler_version.cpp
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
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
  endif ()
else ()
  message(FATAL_ERROR "Your C++ compiler does not support C++11 "
                      "or is not supported")
endif ()

# set build type (evaluate ENABLE_DEBUG flag)
if (ENABLE_DEBUG)
  set(CMAKE_BUILD_TYPE Debug)
  add_definitions(-DCPPA_DEBUG_MODE)
  message(STATUS "Enable libcppa's debug mode")
endif (ENABLE_DEBUG)

if (ENABLE_ADDRESS_SANITIZER)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
  message(STATUS "Enable address sanitizer")
endif (ENABLE_ADDRESS_SANITIZER)

if (CPPA_LOG_LEVEL)
  add_definitions(-DCPPA_LOG_LEVEL=${CPPA_LOG_LEVEL})
endif(CPPA_LOG_LEVEL)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# set build default build type if not set
if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif ("${CMAKE_BUILD_TYPE}" STREQUAL "")

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  set(LIBCPPA_PLATFORM_SRC src/middleman_event_handler_epoll.cpp)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(LIBCPPA_PLATFORM_SRC src/middleman_event_handler_poll.cpp)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Window")
  set(LIBCPPA_PLATFORM_SRC src/middleman_event_handler_poll.cpp src/execinfo_windows.cpp)
else ()
  message(FATAL_ERROR "This platform is not supported by libcppa")
endif()

# get header files; only needed by CMake generators,
# e.g., for creating proper Xcode projects
file(GLOB LIBCPPA_HDRS "cppa/*.hpp"
                       "cppa/detail/*.hpp"
                       "cppa/intrusive/*.hpp"
                       "cppa/io/*.hpp"
                       "cppa/opencl/*.hpp"
                       "cppa/qtsupport/*.hpp"
                       "cppa/util/*.hpp")

# list cpp files including platform-dependent files
set(LIBCPPA_SRC
    ${LIBCPPA_PLATFORM_SRC}
    src/abstract_actor.cpp
    src/abstract_channel.cpp
    src/abstract_group.cpp
    src/abstract_tuple.cpp
    src/acceptor.cpp
    src/actor.cpp
    src/actor_addr.cpp
    src/actor_companion.cpp
    src/actor_namespace.cpp
    src/actor_proxy.cpp
    src/actor_registry.cpp
    src/actor_ostream.cpp
    src/algorithm.cpp
    src/any_tuple.cpp
    src/atom.cpp
    src/attachable.cpp
    src/behavior.cpp
    src/behavior_stack.cpp
    src/binary_deserializer.cpp
    src/binary_serializer.cpp
    src/broker.cpp
    src/buffer.cpp
    src/blocking_actor.cpp
    src/channel.cpp
    src/context_switching_resume.cpp
    src/continuable.cpp
    src/continue_helper.cpp
    src/cs_thread.cpp
    src/decorated_tuple.cpp
    src/actor_proxy.cpp
    src/peer.cpp
    src/peer_acceptor.cpp
    src/demangle.cpp
    src/deserializer.cpp
    src/duration.cpp
    src/empty_tuple.cpp
    src/event_based_actor.cpp
    src/exception.cpp
    src/execution_unit.cpp
    src/exit_reason.cpp
    src/fd_util.cpp
    src/functor_based_actor.cpp
    src/functor_based_blocking_actor.cpp
    src/get_root_uuid.cpp
    src/get_mac_addresses.cpp
    src/group.cpp
    src/group_manager.cpp
    src/input_stream.cpp
    src/ipv4_acceptor.cpp
    src/ipv4_io_stream.cpp
    src/local_actor.cpp
    src/logging.cpp
    src/mailbox_element.cpp
    src/match.cpp
    src/memory.cpp
    src/memory_managed.cpp
    src/message_header.cpp
    src/middleman.cpp
    src/middleman_event_handler.cpp
    src/node_id.cpp
    src/object.cpp
    src/object_array.cpp
    src/opt.cpp
    src/output_stream.cpp
    src/partial_function.cpp
    src/primitive_variant.cpp
    src/ref_counted.cpp
    src/resumable.cpp
    src/remote_actor_proxy.cpp
    src/response_promise.cpp
    src/ripemd_160.cpp
    src/scheduler.cpp
    src/scoped_actor.cpp
    src/serializer.cpp
    src/shared_spinlock.cpp
    src/singleton_manager.cpp
    src/stream.cpp
    src/string_serialization.cpp
    src/sync_request_bouncer.cpp
    src/to_uniform_name.cpp
    src/type_lookup_table.cpp
    src/unicast_network.cpp
    src/uniform_type_info.cpp
    src/uniform_type_info_map.cpp
    src/weak_ptr_anchor.cpp
    src/yield_interface.cpp)

if (BOOST_ROOT)
  # Prevent falling back to system paths when using a custom Boost prefix.
  set(Boost_NO_SYSTEM_PATHS true)
endif ()

set(INCLUDE_DIRS .)
set(LD_DIRS)
set(LD_FLAGS ${CMAKE_LD_LIBS})

if (APPLE)
  # -pthread is ignored on MacOSX
elseif (UNIX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
elseif (MINGW)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
  set(LD_FLAGS "ws2_32 -liphlpapi")
endif ()

if (ENABLE_OPENCL)
  find_package(OPENCL REQUIRED)
  set(LD_FLAGS ${LD_FLAGS} ${OPENCL_LIBRARIES})
  set(INCLUDE_DIRS ${INCLUDE_DIRS} ${OPENCL_INCLUDE_DIR})
  set(LIBCPPA_SRC
      ${LIBCPPA_SRC} 
      src/opencl/global.cpp
      src/opencl/program.cpp
      src/opencl/opencl_metainfo.cpp)
  add_definitions(-DCPPA_OPENCL)
endif (ENABLE_OPENCL)

if (DISABLE_MEM_MANAGEMENT)
  add_definitions(-DCPPA_DISABLE_MEM_MANAGEMENT)
endif (DISABLE_MEM_MANAGEMENT)

if (DISABLE_CONTEXT_SWITCHING)
  # explicitly disabled
else (DISABLE_CONTEXT_SWITCHING)
  find_package(Boost 1.51.0 COMPONENTS context)
  # without REQUIRED, find_package(Boost...) returns with Boost_FOUND=FALSE
  if (NOT Boost_FOUND)
    # disable implicitly
    set(DISABLE_CONTEXT_SWITCHING true)
  else (NOT Boost_FOUND)
    if (${Boost_VERSION} LESS 105400)
      set(LD_FLAGS ${LD_FLAGS} ${Boost_CONTEXT_LIBRARY})
      # nothing to do in this case
    else ()
      # for boost >= 1.54, we need coroutines as well
      message(STATUS "Detected Boost >= 1.54.0, check for coroutines & system")
      find_package(Boost 1.51.0 COMPONENTS system context coroutine)
      if (Boost_FOUND)
        set(LD_FLAGS ${LD_FLAGS} ${Boost_COROUTINE_LIBRARY} ${Boost_CONTEXT_LIBRARY} ${Boost_SYSTEM_LIBRARY})
      endif ()
    endif ()
    # check again, as second find_package might set this to false
    if (Boost_FOUND)
      # This hack fixes a problem when the linker search path is the same as
      # the one provided by the compiler. In this case, CMake replaces the absolute
      # path (e.g., /path/to/lib.so) with -l<lib>, which may cause it to pick up the wrong
      # library. So when this replacement happens, we ensure that the right
      # library gets picked by adding a -L directive for the affected libraries
      # (which is just Boost Context here).
      set(CMAKE_EXE_LINKER_FLAGS -L${Boost_LIBRARY_DIRS})
      set(INCLUDE_DIRS ${INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})
      set(LD_DIRS ${LD_DIRS} ${Boost_LIBRARY_DIRS})
      set(DISABLE_CONTEXT_SWITCHING false)
    else ()
      # disable implicitly
      set(DISABLE_CONTEXT_SWITCHING true)
    endif ()
  endif (NOT Boost_FOUND)
endif (DISABLE_CONTEXT_SWITCHING)

# set compiler flag when compiling w/o context-switching actors
if (DISABLE_CONTEXT_SWITCHING)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCPPA_DISABLE_CONTEXT_SWITCHING")
endif ()

# build shared library if not compiling static only
if (NOT "${CPPA_BUILD_STATIC_ONLY}" STREQUAL "yes")
  add_library(libcppa SHARED ${LIBCPPA_SRC} ${LIBCPPA_HDRS})
  target_link_libraries(libcppa ${LD_FLAGS})
  set(LIBRARY_VERSION ${LIBCPPA_VERSION_MAJOR}.${LIBCPPA_VERSION_MINOR}.${LIBCPPA_VERSION_PATCH})
  set(LIBRARY_SOVERSION ${LIBCPPA_VERSION_MAJOR})
  set_target_properties(libcppa 
                        PROPERTIES
                        SOVERSION ${LIBRARY_SOVERSION}
                        VERSION ${LIBRARY_VERSION}
                        OUTPUT_NAME cppa)
  if(NOT MINGW)
    install(TARGETS libcppa LIBRARY DESTINATION lib)
  endif()
endif ()

# build static library only if --build-static or --build-static-only was set
if ("${CPPA_BUILD_STATIC_ONLY}" STREQUAL "yes" OR "${CPPA_BUILD_STATIC}" STREQUAL "yes")
  add_library(libcppaStatic STATIC ${LIBCPPA_HDRS} ${LIBCPPA_SRC})
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

# setting path to cppa headers and libcppa
set (CPPA_INCLUDE_PATH ${CMAKE_SOURCE_DIR}/libcppa)
set (CPPA_INCLUDE ${CPPA_INCLUDE_PATH})

if ("${CPPA_BUILD_STATIC_ONLY}" STREQUAL "yes")
  set (CPPA_LIBRARY ${LIBRARY_OUTPUT_PATH}/libcppa_static.a ${LD_FLAGS})
else ()
  if (APPLE)
    if ("${CMAKE_GENERATOR}" STREQUAL "Xcode")
      set (CPPA_LIBRARY "-lcppa")
    else ()
      set (CPPA_LIBRARY ${LIBRARY_OUTPUT_PATH}/libcppa.dylib)
    endif ()
    #set (CPPA_LIBRARY -L${LIBRARY_OUTPUT_PATH} -lcppa)
  elseif (UNIX)
    set (CPPA_LIBRARY ${LIBRARY_OUTPUT_PATH}/libcppa.so)
  elseif (MINGW)
    set (CPPA_LIBRARY ${LIBRARY_OUTPUT_PATH}/libcppa.dll)
  else ()
    message (SEND_FATAL "Host platform not supported ...")
  endif ()
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
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
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
toYesNo(ENABLE_OPENCL BUILD_OPENCL_STR)
toYesNo(DISABLE_MEM_MANAGEMENT DISABLE_MEM_MANAGEMENT_STR)
invertYesNo(CPPA_NO_EXAMPLES BUILD_EXAMPLES)
invertYesNo(CPPA_NO_UNIT_TESTS BUILD_UNIT_TESTS)
invertYesNo(DISABLE_MEM_MANAGEMENT_STR WITH_MEM_MANAGEMENT)

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
        "\nBuild OpenCL:      ${BUILD_OPENCL_STR}"
        "\nWith mem. mgmt.:   ${WITH_MEM_MANAGEMENT}"
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
