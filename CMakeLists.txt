cmake_minimum_required(VERSION 2.8.4)
project(caf C CXX)

# be nice to vim users
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# silence policy CMP0042 warning by enabling RPATH explicitly
if(APPLE AND NOT DEFINED CMAKE_MACOSX_RPATH)
  set(CMAKE_MACOSX_RPATH true)
endif()

# shared libs currently not supported on Windows
if(WIN32 AND NOT CAF_BUILD_STATIC_ONLY)
  message(STATUS "CAF currently only supports static-only builds on Windows")
  set(CAF_BUILD_STATIC_ONLY yes)
endif()

# add helper target that simplifies re-running configure
add_custom_target(configure COMMAND ${CMAKE_CURRENT_BINARY_DIR}/config.status)

################################################################################
#                        set included flags if needed                          #
################################################################################

if(CAF_FORCE_NO_EXCEPTIONS)
  set(CAF_NO_EXCEPTIONS yes)
endif()

################################################################################
#                        set prefix paths if available                         #
################################################################################

if(DEFINED CAF_QT_PREFIX_PATH)
  set(CMAKE_PREFIX_PATH "${CAF_QT_PREFIX_PATH}")
endif()

################################################################################
#                      enable ccache if required by user                       #
################################################################################

if(CAF_USE_CCACHE)
  find_program(CCACHE_PROGRAM ccache)
  if(CCACHE_PROGRAM)
    message(STATUS "Using ccache command: ${CCACHE_PROGRAM}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CCACHE_PROGRAM}")
  else()
    message(STATUS "Unable to find ccache")
  endif()
endif(CAF_USE_CCACHE)

################################################################################
#   make sure all variables are set to "no" if undefined for summary output    #
################################################################################

if(NOT CAF_ENABLE_RUNTIME_CHECKS)
  set(CAF_ENABLE_RUNTIME_CHECKS no)
endif()

if(NOT CAF_NO_MEM_MANAGEMENT)
  set(CAF_NO_MEM_MANAGEMENT no)
endif()

if(NOT CAF_NO_EXCEPTIONS)
  set(CAF_NO_EXCEPTIONS no)
endif()

if(NOT CAF_BUILD_STATIC_ONLY)
  set(CAF_BUILD_STATIC_ONLY no)
endif()

if(NOT CAF_BUILD_STATIC)
  set(CAF_BUILD_STATIC no)
endif()

if(NOT CAF_NO_OPENCL)
  set(CAF_NO_OPENCL no)
endif()

if(NOT CAF_NO_PYTHON)
  set(CAF_NO_PYTHON no)
endif()

if(NOT CAF_NO_TOOLS)
  set(CAF_NO_TOOLS no)
endif()

if(NOT CAF_NO_SUMMARY)
  set(CAF_NO_SUMMARY no)
endif()

if(NOT CAF_NO_IO)
  set(CAF_NO_IO no)
else()
  set(CAF_NO_TOOLS yes)
  set(CAF_NO_PYTHON yes)
endif()

################################################################################
#                              get version of CAF                              #
################################################################################

# read content of config.hpp
file(READ "libcaf_core/caf/config.hpp" CONFIG_HPP)
# get line containing the version
string(REGEX MATCH "#define CAF_VERSION [0-9]+" VERSION_LINE "${CONFIG_HPP}")
# extract version number from line
string(REGEX MATCH "[0-9]+" VERSION_INT "${VERSION_LINE}")
# calculate major, minor, and patch version
math(EXPR CAF_VERSION_MAJOR "${VERSION_INT} / 10000")
math(EXPR CAF_VERSION_MINOR "( ${VERSION_INT} / 100) % 100")
math(EXPR CAF_VERSION_PATCH "${VERSION_INT} % 100")
# create full version string
set(CAF_VERSION
    "${CAF_VERSION_MAJOR}.${CAF_VERSION_MINOR}.${CAF_VERSION_PATCH}")


################################################################################
#   set output paths for binaries and libraries if not provided by the user    #
################################################################################

# prohibit in-source builds
if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
    message(FATAL_ERROR "In-source builds are not allowed. Please use "
                        "./configure to choose a build directory and "
                        "initialize the build configuration.")
endif()
# set module path appropriately
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
# set binary output path if not defined by user
if("${EXECUTABLE_OUTPUT_PATH}" STREQUAL "")
  set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/bin")
endif()
# set library output path if not defined by user, but always set
# library output path to binary output path for Xcode projects
if("${CMAKE_GENERATOR}" STREQUAL "Xcode")
  set(LIBRARY_OUTPUT_PATH "${EXECUTABLE_OUTPUT_PATH}")
elseif("${LIBRARY_OUTPUT_PATH}" STREQUAL "")
  set(LIBRARY_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/lib")
endif()


################################################################################
#                                compiler setup                                #
################################################################################

# check for g++ >= 4.8 or clang++ > = 3.2
if(NOT WIN32 AND NOT CAF_NO_COMPILER_CHECK)
  try_run(ProgramResult
          CompilationSucceeded
          "${CMAKE_CURRENT_BINARY_DIR}"
          "${CMAKE_CURRENT_SOURCE_DIR}/cmake/get_compiler_version.cpp"
          RUN_OUTPUT_VARIABLE CompilerVersion)
  if(NOT CompilationSucceeded OR NOT ProgramResult EQUAL 0)
    message(FATAL_ERROR "Cannot determine compiler version")
  elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
    if(CompilerVersion VERSION_GREATER 4.7)
      message(STATUS "Found g++ version ${CompilerVersion}")
    else()
      message(FATAL_ERROR "g++ >= 4.8 required (found: ${CompilerVersion})")
    endif()
  elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    if(CompilerVersion VERSION_GREATER 3.1)
      message(STATUS "Found clang++ version ${CompilerVersion}")
    else()
      message(FATAL_ERROR "clang++ >= 3.2 required (found: ${CompilerVersion})")
    endif()
  else()
    message(FATAL_ERROR "Your C++ compiler does not support C++11 "
                        "or is not supported")
  endif()
endif()
# set optional build flags
set(EXTRA_FLAGS "")
# increase max. template depth on GCC and Clang
if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang"
   OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
  set(EXTRA_FLAGS "-ftemplate-depth=512 -ftemplate-backtrace-limit=0")
endif()
# explicitly disable obnoxious GCC warnings
if("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
  set(EXTRA_FLAGS "-Wno-missing-field-initializers")
endif()
# add "-Werror" flag if --pedantic-build is used
if(CAF_CXX_WARNINGS_AS_ERRORS)
  set(EXTRA_FLAGS "${EXTRA_FLAGS} -Werror")
endif()
# set compiler flags for GCOV if requested
if(CAF_ENABLE_GCOV)
  if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(NO_INLINE "-fno-inline")
  else()
    set(NO_INLINE "-fno-inline -fno-inline-small-functions -fno-default-inline")
  endif()
  set(EXTRA_FLAGS "${EXTRA_FLAGS} -fprofile-arcs -ftest-coverage ${NO_INLINE}")
endif()
# set -fno-exception if requested
if(CAF_FORCE_NO_EXCEPTIONS)
  set(EXTRA_FLAGS "${EXTRA_FLAGS} -fno-exceptions")
endif()
# enable a ton of warnings if --more-clang-warnings is used
if(CAF_MORE_WARNINGS)
  if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(WFLAGS "-Weverything -Wno-c++98-compat -Wno-padded "
               "-Wno-documentation-unknown-command -Wno-exit-time-destructors "
               "-Wno-global-constructors -Wno-missing-prototypes "
               "-Wno-c++98-compat-pedantic -Wno-unused-member-function "
               "-Wno-unused-const-variable -Wno-switch-enum "
               "-Wno-missing-noreturn -Wno-covered-switch-default")
  elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
    set(WFLAGS "-Waddress -Wall -Warray-bounds "
               "-Wattributes -Wbuiltin-macro-redefined -Wcast-align "
               "-Wcast-qual -Wchar-subscripts -Wclobbered -Wcomment "
               "-Wconversion -Wconversion-null -Wcoverage-mismatch "
               "-Wcpp -Wdelete-non-virtual-dtor -Wdeprecated "
               "-Wdeprecated-declarations -Wdiv-by-zero -Wdouble-promotion "
               "-Wempty-body -Wendif-labels -Wenum-compare -Wextra "
               "-Wfloat-equal -Wformat -Wfree-nonheap-object "
               "-Wignored-qualifiers -Winit-self "
               "-Winline -Wint-to-pointer-cast -Winvalid-memory-model "
               "-Winvalid-offsetof -Wlogical-op -Wmain -Wmaybe-uninitialized "
               "-Wmissing-braces -Wmultichar "
               "-Wnarrowing -Wnoexcept -Wnon-template-friend "
               "-Wnon-virtual-dtor -Wnonnull -Woverflow "
               "-Woverlength-strings -Wparentheses "
               "-Wpmf-conversions -Wpointer-arith -Wreorder "
               "-Wreturn-type -Wsequence-point -Wshadow "
               "-Wsign-compare -Wswitch -Wtype-limits -Wundef "
               "-Wuninitialized -Wunused -Wvla -Wwrite-strings")
  endif()
  # convert CMake list to a single string, erasing the ";" separators
  string(REPLACE ";" "" WFLAGS_STR ${WFLAGS})
  set(EXTRA_FLAGS "${EXTRA_FLAGS} ${WFLAGS_STR}")
endif()
# add -stdlib=libc++ when using Clang if possible
if(NOT CAF_NO_AUTO_LIBCPP AND "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  set(CXXFLAGS_BACKUP "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "-std=c++11 -stdlib=libc++")
  try_run(ProgramResult
          CompilationSucceeded
          "${CMAKE_CURRENT_BINARY_DIR}"
          "${CMAKE_CURRENT_SOURCE_DIR}/cmake/get_compiler_version.cpp"
          RUN_OUTPUT_VARIABLE CompilerVersion)
  if(NOT CompilationSucceeded OR NOT ProgramResult EQUAL 0)
    message(STATUS "Use clang with GCC' libstdc++")
  else()
    message(STATUS "Automatically added '-stdlib=libc++' flag "
                   "(CAF_NO_AUTO_LIBCPP not defined)")
    set(EXTRA_FLAGS "${EXTRA_FLAGS} -stdlib=libc++")
  endif()
  # restore CXX flags
  set(CMAKE_CXX_FLAGS "${CXXFLAGS_BACKUP}")
endif()
# enable address sanitizer if requested by the user
if(CAF_ENABLE_ADDRESS_SANITIZER)
  # check whether address sanitizer is available
  set(CXXFLAGS_BACKUP "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "-fsanitize=address -fno-omit-frame-pointer")
  try_run(ProgramResult
          CompilationSucceeded
          "${CMAKE_CURRENT_BINARY_DIR}"
          "${CMAKE_CURRENT_SOURCE_DIR}/cmake/get_compiler_version.cpp")
  if(NOT CompilationSucceeded)
    message(WARNING "Address Sanitizer is not available on selected compiler")
  else()
    message(STATUS "Enable Address Sanitizer")
    set(EXTRA_FLAGS "${EXTRA_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
  endif()
  # restore CXX flags
  set(CMAKE_CXX_FLAGS "${CXXFLAGS_BACKUP}")
endif(CAF_ENABLE_ADDRESS_SANITIZER)
# -pthread is ignored on MacOSX but required on other platforms
if(NOT APPLE AND NOT WIN32)
  set(EXTRA_FLAGS "${EXTRA_FLAGS} -pthread")
endif()
# -fPIC generates warnings on MinGW and Cygwin plus extra setup steps needed on MinGW
if(MINGW)
  add_definitions(-D_WIN32_WINNT=0x0600)
  add_definitions(-DWIN32)
  include(GenerateExportHeader)
  set(LD_FLAGS "ws2_32 -liphlpapi")
  # build static to avoid runtime dependencies to GCC libraries
  set(EXTRA_FLAGS "${EXTRA_FLAGS} -static")
elseif(CYGWIN)
  set(EXTRA_FLAGS "${EXTRA_FLAGS} -U__STRICT_ANSI__")
else()
  set(EXTRA_FLAGS "${EXTRA_FLAGS} -fPIC")
endif()
if (WIN32)
  set(LD_FLAGS ${LD_FLAGS} ws2_32 iphlpapi)
endif()
# iOS support
if(CAF_OSX_SYSROOT)
  set(CMAKE_OSX_SYSROOT "${CAF_OSX_SYSROOT}")
endif()
if(CAF_IOS_DEPLOYMENT_TARGET)
  if("${CAF_OSX_SYSROOT}" STREQUAL "iphonesimulator")
    set(EXTRA_FLAGS "${EXTRA_FLAGS} -mios-simulator-version-min=${CAF_IOS_DEPLOYMENT_TARGET}")
  else()
    set(EXTRA_FLAGS "${EXTRA_FLAGS} -miphoneos-version-min=${CAF_IOS_DEPLOYMENT_TARGET}")
  endif()
endif()
# check if the user provided CXXFLAGS, set defaults otherwise
if(NOT CMAKE_CXX_FLAGS)
  set(CMAKE_CXX_FLAGS                   "-std=c++11 -Wextra -Wall -pedantic ${EXTRA_FLAGS}")
endif()
if(NOT CMAKE_CXX_FLAGS_DEBUG)
  set(CMAKE_CXX_FLAGS_DEBUG             "-O0 -g")
endif()
if(NOT CMAKE_CXX_FLAGS_MINSIZEREL)
  set(CMAKE_CXX_FLAGS_MINSIZEREL        "-Os")
endif()
if(NOT CMAKE_CXX_FLAGS_RELEASE)
  set(CMAKE_CXX_FLAGS_RELEASE           "-O3 -DNDEBUG")
endif()
if(NOT CMAKE_CXX_FLAGS_RELWITHDEBINFO)
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO    "-O2 -g -fno-omit-frame-pointer")
endif()
# set build default build type to RelWithDebInfo if not set
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
# needed by subprojects
set(LD_FLAGS ${LD_FLAGS} ${CMAKE_LD_LIBS})

################################################################################
#                       configure build_config.hpp header                      #
################################################################################

if(CAF_LOG_LEVEL)
  set(CAF_LOG_LEVEL_INT ${CAF_LOG_LEVEL})
else()
  set(CAF_LOG_LEVEL_INT -1)
endif()

macro(to_int_value name)
  if(${name})
    set(${name}_INT 1)
  else()
    set(${name}_INT -1)
  endif()
endmacro()

to_int_value(CAF_LOG_LEVEL)
to_int_value(CAF_NO_EXCEPTIONS)
to_int_value(CAF_NO_MEM_MANAGEMENT)
to_int_value(CAF_ENABLE_RUNTIME_CHECKS)

# find boost asio if the asio multiplexer should be used for testing
if(CAF_USE_ASIO)
  set(Boost_USE_MULTITHREADED ON)
  find_package(Boost COMPONENTS system)
  if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    set(LD_DIRS ${LD_DIRS} ${Boost_LIBRARIES})
    set(LD_FLAGS ${LD_FLAGS} ${Boost_SYSTEM_LIBRARY})
    set(CAF_USE_ASIO_INT 1)
  else()
    set(CAF_USE_ASIO no)
    set(CAF_USE_ASIO_INT -1)
  endif()
else()
  set(CAF_USE_ASIO_INT -1)
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/build_config.hpp.in"
               "${CMAKE_CURRENT_SOURCE_DIR}/libcaf_core/caf/detail/build_config.hpp"
               IMMEDIATE @ONLY)


################################################################################
#                           setup for install target                           #
################################################################################

# install includes from core
install(DIRECTORY libcaf_core/caf/
        DESTINATION include/caf FILES_MATCHING PATTERN "*.hpp")
# install includes from io
install(DIRECTORY libcaf_io/caf/ DESTINATION include/caf
        FILES_MATCHING PATTERN "*.hpp")
# install includes from test
install(DIRECTORY libcaf_test/caf/ DESTINATION include/caf
        FILES_MATCHING PATTERN "*.hpp")
# install includes from opencl
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libcaf_opencl/CMakeLists.txt")
  install(DIRECTORY libcaf_opencl/caf/ DESTINATION include/caf
        FILES_MATCHING PATTERN "*.hpp")
endif()

# process cmake_uninstall.cmake.in
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
               "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
               IMMEDIATE @ONLY)
# add uninstall target
add_custom_target(uninstall
                  COMMAND "${CMAKE_COMMAND}" -P
                  "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")


################################################################################
#                       set inclue paths for subprojects                       #
################################################################################

# path to caf core & io headers
set(LIBCAF_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/libcaf_core"
    "${CMAKE_CURRENT_SOURCE_DIR}/libcaf_io"
    "${CMAKE_CURRENT_SOURCE_DIR}/libcaf_test")
# path to caf opencl headers
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libcaf_opencl/CMakeLists.txt")
  set(LIBCAF_INCLUDE_DIRS
      "${CMAKE_CURRENT_SOURCE_DIR}/libcaf_opencl/" "${LIBCAF_INCLUDE_DIRS}")
endif()
# enable tests if not disabled
if(NOT CAF_NO_UNIT_TESTS)
  enable_testing()
  macro(add_unit_tests globstr)
    file(GLOB_RECURSE tests "${globstr}")
    set(CAF_ALL_UNIT_TESTS ${CAF_ALL_UNIT_TESTS} ${tests})
  endmacro()
else()
  macro(add_unit_tests globstr)
    # do nothing (unit tests disabled)
  endmacro()
endif()
# all projects need the headers of the core components
include_directories("${LIBCAF_INCLUDE_DIRS}")


################################################################################
#                                 add targets                                  #
################################################################################

macro(add_caf_lib name)
  string(TOUPPER ${name} upper_name)
  set(full_name libcaf_${name})
  set(shared_target ${full_name}_shared)
  set(static_target ${full_name}_static)
  add_subdirectory(${full_name})
  set(lib_varname CAF_LIBRARY_${upper_name})
  set(lib_varname_static ${lib_varname}_STATIC)
  if(NOT CAF_BUILD_STATIC_ONLY)
    set(${lib_varname} ${shared_target})
    set(CAF_LIBRARIES ${CAF_LIBRARIES} ${shared_target})
  else()
    set(${lib_varname} ${static_target})
    set(CAF_LIBRARIES ${CAF_LIBRARIES} ${static_target})
  endif()
  if(CAF_BUILD_STATIC_ONLY OR CAF_BUILD_STATIC)
    set(${lib_varname_static} ${static_target})
  endif()
  add_unit_tests("${full_name}/test/*.cpp")
  # add headers to include directories so other subprojects can use them
  include_directories("${CMAKE_CURRENT_SOURCE_DIR}/libcaf_${name}")
endmacro()

macro(add_optional_caf_lib name)
  string(TOUPPER ${name} upper_name)
  set(flag_varname CAF_NO_${upper_name})
  if(NOT ${flag_varname}
     AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libcaf_${name}/CMakeLists.txt")
    add_caf_lib(${name})
  else()
    set(${flag_name} yes)
  endif()
endmacro()

macro(add_optional_caf_binaries name)
  string(TOUPPER ${name} upper_name)
  set(dependency_failed no)
  # check all aditional dependency flags
  foreach(flag_name ${ARGN})
    if(${flag_name})
      set(dependency_failed yes)
    endif()
  endforeach()
  if(NOT dependency_failed)
    if(NOT CAF_NO_${upper_name}
       AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${name}/CMakeLists.txt")
      add_subdirectory(${name})
    else()
      # make sure variable is set for nicer build log
      set(CAF_NO_${upper_name} yes)
    endif()
  else()
    message(STATUS
            "Disable ${name}, one of the following flags was set: ${ARGN}")
    # make sure variable is set for nicer build log
    set(CAF_NO_${upper_name} yes)
  endif()
endmacro()

# build core and I/O library
add_caf_lib(core)
add_optional_caf_lib(io)

# build SSL module if OpenSSL library is available
find_package(OpenSSL)
if(OPENSSL_FOUND)
  include_directories(BEFORE ${OPENSSL_INCLUDE_DIR})
  add_optional_caf_lib(openssl)
endif(OPENSSL_FOUND)

# build opencl library if not told otherwise and OpenCL package was found
if(NOT CAF_NO_OPENCL)
  find_package(OpenCL)
  if(OpenCL_FOUND)
    add_optional_caf_lib(opencl)
    add_optional_caf_binaries(libcaf_opencl/examples)
  endif()
endif()

# build Python binding if not being told otherwise
if(NOT CAF_NO_PYTHON AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libcaf_python/CMakeLists.txt")
  add_subdirectory(libcaf_python)
endif()

# build examples if not being told otherwise
add_optional_caf_binaries(examples)

# build tools if not being told otherwise
add_optional_caf_binaries(tools)

# build benchmarks if not being told otherwise
add_optional_caf_binaries(benchmarks)


################################################################################
#                               unit tests setup                               #
################################################################################

if(NOT CAF_NO_UNIT_TESTS)
  # setup unit test binary
  add_executable(caf-test
                 libcaf_test/src/caf-test.cpp
                 libcaf_test/caf/test/unit_test.hpp
                 libcaf_test/caf/test/unit_test_impl.hpp
                 ${CAF_ALL_UNIT_TESTS})
  target_link_libraries(caf-test
                        ${LD_FLAGS}
                        ${CAF_LIBRARIES}
                        ${PTHREAD_LIBRARIES})
  add_custom_target(all_unit_tests)
  add_dependencies(caf-test all_unit_tests)
  # enumerate all test suites.
  foreach(test ${CAF_ALL_UNIT_TESTS})
    file(STRINGS ${test} contents)
    foreach(line ${contents})
      if ("${line}" MATCHES "CAF_SUITE (.*)")
        string(REGEX REPLACE ".* CAF_SUITE (.*)" "\\1" suite ${line})
        list(APPEND suites ${suite})
      endif()
    endforeach()
  endforeach()
  list(REMOVE_DUPLICATES suites)
  # creates one CMake test per test suite.
  macro (make_test suite)
    string(REPLACE " " "_" test_name ${suite})
    set(caf_test ${EXECUTABLE_OUTPUT_PATH}/caf-test)
    add_test(${test_name} ${caf_test} -n -v 5 -s "${suite}" ${ARGN})
    # add some extra unit testing options when testing ASIO as well
    if(CAF_USE_ASIO AND "${suite}" MATCHES "^io_.+$")
      add_test(${test_name}_asio ${caf_test} -n -v 5 -s
               "${suite}" ${ARGN} -- "--caf#middleman.network-backend=asio")
    endif()
  endmacro ()
  list(LENGTH suites num_suites)
  message(STATUS "Found ${num_suites} test suites")
  foreach(suite ${suites})
    make_test("${suite}")
  endforeach ()
endif()


################################################################################
#                                Doxygen setup                                 #
################################################################################

# check for doxygen and add custom "doc" target to Makefile
find_package(Doxygen)
if(DOXYGEN_FOUND)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile.in"
                 "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile"
                 @ONLY)
  add_custom_target(doc "${DOXYGEN_EXECUTABLE}"
                    "${CMAKE_HOME_DIRECTORY}/Doxyfile"
                    WORKING_DIRECTORY "${CMAKE_HOME_DIRECTORY}"
                    COMMENT "Generating API documentation with Doxygen"
                    VERBATIM)
endif(DOXYGEN_FOUND)

################################################################################
#                                Manual setup                                  #
################################################################################

set(MANUAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/manual")
if(EXISTS "${MANUAL_DIR}"
   AND EXISTS "${MANUAL_DIR}/variables.tex.in"
   AND EXISTS "${MANUAL_DIR}/conf.py.in")
  # retrieve current branch name for CAF
  execute_process(COMMAND git rev-parse --abbrev-ref HEAD
                  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                  OUTPUT_VARIABLE GIT_BRANCH
                  OUTPUT_STRIP_TRAILING_WHITESPACE)
  # retrieve current SHA1 hash for CAF
  execute_process(COMMAND git log --pretty=format:%h -n 1
                  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                  OUTPUT_VARIABLE CAF_SHA
                  OUTPUT_STRIP_TRAILING_WHITESPACE)
  # retrieve current SHA1 hash for the Manual
  execute_process(COMMAND git log --pretty=format:%h -n 1
                  WORKING_DIRECTORY ${MANUAL_DIR}
                  OUTPUT_VARIABLE CAF_MANUAL_SHA
                  OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(${GIT_BRANCH} STREQUAL "master")
    # retrieve current tag name for CAF
    execute_process(COMMAND git describe --tags --contains ${CAF_SHA}
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                    RESULT_VARIABLE GIT_TAG_RES
                    OUTPUT_VARIABLE GIT_TAG
                    ERROR_QUIET
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    # prefer tag name over auto-generated version
    if("${GIT_TAG_RES}" STREQUAL "0")
      set(CAF_RELEASE "${GIT_TAG}")
    else()
      set(CAF_RELEASE "${CAF_VERSION}")
    endif()
  else()
    set(CAF_RELEASE "${CAF_VERSION}+exp.sha.${CAF_SHA}")
  endif()
  MESSAGE(STATUS "Set manual version to ${CAF_RELEASE}")
  configure_file("${MANUAL_DIR}/variables.tex.in"
                 "${MANUAL_DIR}/variables.tex"
                 IMMEDIATE @ONLY)
  configure_file("${MANUAL_DIR}/conf.py.in"
                 "${MANUAL_DIR}/conf.py"
                 IMMEDIATE @ONLY)
  configure_file("${MANUAL_DIR}/index_header.rst.in"
                 "${MANUAL_DIR}/index_header.rst"
                 IMMEDIATE @ONLY)
  configure_file("${MANUAL_DIR}/index_footer.rst.in"
                 "${MANUAL_DIR}/index_footer.rst"
                 IMMEDIATE @ONLY)
endif()

################################################################################
#                     Add additional project files to GUI                      #
################################################################################

file(GLOB_RECURSE script_files "scripts/*")
add_custom_target(gui_dummy SOURCES configure ${script_files})

################################################################################
#                                print summary                                 #
################################################################################

# set human-readable representation for log level
set(LOG_LEVEL_STR "none")
if(CAF_LOG_LEVEL)
  if(${CAF_LOG_LEVEL} EQUAL 0)
    set(LOG_LEVEL_STR "ERROR")
  elseif(${CAF_LOG_LEVEL} EQUAL 1)
    set(LOG_LEVEL_STR "WARNING")
  elseif(${CAF_LOG_LEVEL} EQUAL 2)
    set(LOG_LEVEL_STR "INFO")
  elseif(${CAF_LOG_LEVEL} EQUAL 3)
    set(LOG_LEVEL_STR "DEBUG")
  elseif(${CAF_LOG_LEVEL} EQUAL 4)
    set(LOG_LEVEL_STR "TRACE")
  else()
    set(LOG_LEVEL_STR "invalid")
  endif()
endif()
# little helper macro to invert a boolean
macro(invertYesNo in out)
  if(${in})
    set(${out} no)
  else()
    set(${out} yes)
  endif()
endmacro()
# invert CAF_NO_* variables for nicer output
invertYesNo(CAF_NO_IO CAF_BUILD_IO)
invertYesNo(CAF_NO_EXAMPLES CAF_BUILD_EXAMPLES)
invertYesNo(CAF_NO_TOOLS CAF_BUILD_TOOLS)
invertYesNo(CAF_NO_UNIT_TESTS CAF_BUILD_UNIT_TESTS)
invertYesNo(CAF_NO_EXCEPTIONS CAF_BUILD_WITH_EXCEPTIONS)
invertYesNo(CAF_NO_MEM_MANAGEMENT CAF_BUILD_MEM_MANAGEMENT)
invertYesNo(CAF_NO_BENCHMARKS CAF_BUILD_BENCHMARKS)
invertYesNo(CAF_NO_OPENCL CAF_BUILD_OPENCL)
invertYesNo(CAF_NO_PYTHON CAF_BUILD_PYTHON)
# collect all compiler flags
string(TOUPPER "${CMAKE_BUILD_TYPE}" UPPER_BUILD_TYPE)
set(ALL_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${UPPER_BUILD_TYPE}}")
# done
if(NOT CAF_NO_SUMMARY)
  message(STATUS
        "\n====================|  Build Summary  |===================="
        "\n"
        "\nLibcaf version:    ${CAF_VERSION}"
        "\n"
        "\nBuild type:        ${CMAKE_BUILD_TYPE}"
        "\nBuild static:      ${CAF_BUILD_STATIC}"
        "\nBuild static only: ${CAF_BUILD_STATIC_ONLY}"
        "\nRuntime checks:    ${CAF_ENABLE_RUNTIME_CHECKS}"
        "\nLog level:         ${LOG_LEVEL_STR}"
        "\nWith mem. mgmt.:   ${CAF_BUILD_MEM_MANAGEMENT}"
        "\nWith exceptions:   ${CAF_BUILD_WITH_EXCEPTIONS}"
        "\n"
        "\nBuild I/O module:  ${CAF_BUILD_IO}"
        "\nBuild tools:       ${CAF_BUILD_TOOLS}"
        "\nBuild examples:    ${CAF_BUILD_EXAMPLES}"
        "\nBuild unit tests:  ${CAF_BUILD_UNIT_TESTS}"
        "\nBuild benchmarks:  ${CAF_BUILD_BENCHMARKS}"
        "\nBuild opencl:      ${CAF_BUILD_OPENCL}"
        "\nBuild Python:      ${CAF_BUILD_PYTHON}"
        "\n"
        "\nCXX:               ${CMAKE_CXX_COMPILER}"
        "\nCXXFLAGS:          ${ALL_CXX_FLAGS}"
        "\nLIBRARIES:         ${LD_FLAGS}"
        "\n"
        "\nSource directory:  ${CMAKE_CURRENT_SOURCE_DIR}"
        "\nBuild directory:   ${CMAKE_CURRENT_BINARY_DIR}"
        "\nExecutable path:   ${EXECUTABLE_OUTPUT_PATH}"
        "\nLibrary path:      ${LIBRARY_OUTPUT_PATH}"
        "\nInstall prefix:    ${CMAKE_INSTALL_PREFIX}"
        "\nGenerator:         ${CMAKE_GENERATOR}"
        "\n"
        "\n===========================================================\n")
endif()
