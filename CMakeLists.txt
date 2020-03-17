cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project(caf C CXX)

include(CheckCSourceCompiles)
include(CheckCSourceRuns)

# Set default install paths.
# See https://cmake.org/cmake/help/latest/module/GNUInstallDirs.html
include(GNUInstallDirs)

# Check whether we are running as CMake subdirectory.
get_directory_property(_parent PARENT_DIRECTORY)
if(_parent)
  set(caf_is_subproject ON)
else()
  set(caf_is_subproject OFF)
endif()
unset(_parent)

if(NOT CMAKE_CROSSCOMPILING)
  # Check whether the user already provided flags that enable C++ >= 17.
  try_compile(caf_has_cxx_17
              "${CMAKE_CURRENT_BINARY_DIR}"
              "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check-compiler-features.cpp")
  # Try enabling C++17 mode if user-provided flags aren't sufficient.
  if(NOT caf_has_cxx_17)
    if(MSVC)
      set(cxx_flag "/std:c++17")
    else()
      if(CMAKE_CXX_COMPILER_ID MATCHES "Clang"
         AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5)
        set(cxx_flag "-std=c++1z")
      else()
        set(cxx_flag "-std=c++17")
      endif()
    endif()
    # Re-run compiler check.
    try_compile(caf_has_cxx_17
                "${CMAKE_CURRENT_BINARY_DIR}"
                "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check-compiler-features.cpp"
                COMPILE_DEFINITIONS "${cxx_flag}"
                OUTPUT_VARIABLE cxx_check_output)
    if(NOT caf_has_cxx_17)
      MESSAGE(FATAL_ERROR "\nFatal error: unable activate C++17 mode!\
                           \nPlease see README.md for supported compilers.\
                           \n\ntry_compile output:\n${cxx_check_output}")
    endif()
    add_compile_options("${cxx_flag}")
  endif()
endif()

# Be nice to VIM users and Clang tools.
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# Silence policy CMP0042 warning by enabling RPATH explicitly.
if(APPLE AND NOT DEFINED CMAKE_MACOSX_RPATH)
  set(CMAKE_MACOSX_RPATH true)
endif()

# Shared libs are currently not supported on Windows.
if(WIN32 AND NOT CAF_BUILD_STATIC_ONLY)
  message(STATUS "CAF currently only supports static-only builds on Windows")
  set(CAF_BUILD_STATIC_ONLY yes)
endif()

if(CAF_BUILD_STATIC_RUNTIME)
    set(flags_configs
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_RELWITHDEBINFO
        CMAKE_CXX_FLAGS_MINSIZEREL
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_RELWITHDEBINFO
        CMAKE_C_FLAGS_MINSIZEREL
        )
  foreach(flags ${flags_configs})
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      if(NOT ${flags} MATCHES "-static-libstdc\\+\\+")
        set(${flags} "${${flags}} -static-libstdc++")
      endif()
      if(NOT ${flags} MATCHES "-static-libgcc")
        set(${flags} "${${flags}} -static-libgcc")
      endif()
    elseif(MSVC)
      if(${flags} MATCHES "/MD")
        string(REGEX REPLACE "/MD" "/MT" ${flags} "${${flags}}")
      endif()
    endif()
  endforeach()
else()
  set(CAF_BUILD_STATIC_RUNTIME no)
endif()

# add helper target that simplifies re-running configure
if(NOT caf_is_subproject)
  add_custom_target(configure COMMAND ${CMAKE_CURRENT_BINARY_DIR}/config.status)
endif()

################################################################################
#                              utility functions                               #
################################################################################

# Appends `str` to the variable named `var` with a whitespace as separator.
# Suppresses a leading whitespace if the variable is empty and does nothing if
# `str` is empty.
function(build_string var str)
  if(NOT str STREQUAL "")
    if("${${var}}" STREQUAL "")
      set("${var}" "${str}" PARENT_SCOPE)
    else()
      set("${var}" "${${var}} ${str}" PARENT_SCOPE)
    endif()
  endif()
endfunction(build_string)

# Forces `var` to 'no' if the content of the variables evaluates to false.
function(pretty_no var)
  if(NOT "${${var}}")
    set("${var}" no PARENT_SCOPE)
  endif()
endfunction(pretty_no)

# Forces `var` to 'yes' if the content of the variables evaluates to false.
function(pretty_yes var)
  if("${${var}}")
    set("${var}" yes PARENT_SCOPE)
  endif()
endfunction(pretty_yes)

add_executable(caf-generate-enum-strings
               EXCLUDE_FROM_ALL
               cmake/caf-generate-enum-strings.cpp)

add_custom_target(consistency-check)

add_custom_target(update-enum-strings)

# adds a consistency check that verifies that `cpp_file` is still valid by
# re-generating the file and comparing it to the existing file
function(add_enum_consistency_check hpp_file cpp_file)
  set(input "${CMAKE_CURRENT_SOURCE_DIR}/${hpp_file}")
  set(file_under_test "${CMAKE_CURRENT_SOURCE_DIR}/${cpp_file}")
  set(output "${CMAKE_CURRENT_BINARY_DIR}/check/${cpp_file}")
  get_filename_component(output_dir "${output}" DIRECTORY)
  file(MAKE_DIRECTORY "${output_dir}")
  add_custom_command(OUTPUT "${output}"
                     COMMAND caf-generate-enum-strings "${input}" "${output}"
                     DEPENDS caf-generate-enum-strings "${input}")
  get_filename_component(target_name "${input}" NAME_WE)
  add_custom_target("${target_name}"
                    COMMAND
                      "${CMAKE_COMMAND}"
                      "-Dfile_under_test=${file_under_test}"
                      "-Dgenerated_file=${output}"
                      -P "${PROJECT_SOURCE_DIR}/cmake/check-consistency.cmake"
                    DEPENDS "${output}")
  add_dependencies(consistency-check "${target_name}")
  add_custom_target("${target_name}-update"
                    COMMAND
                      caf-generate-enum-strings
                      "${input}"
                      "${file_under_test}"
                     DEPENDS caf-generate-enum-strings "${input}")
  add_dependencies(update-enum-strings "${target_name}-update")
endfunction()

################################################################################
#                        set prefix paths if available                         #
################################################################################

build_string("CMAKE_PREFIX_PATH" "${CAF_QT_PREFIX_PATH}")

################################################################################
#                      enable ccache if required by user                       #
################################################################################

if(CAF_USE_CCACHE)
  find_program(CCACHE_PROGRAM ccache)
  if(CCACHE_PROGRAM)
    message(STATUS "Using ccache command: ${CCACHE_PROGRAM}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
  else()
    message(STATUS "Unable to find ccache")
  endif()
endif(CAF_USE_CCACHE)

################################################################################
#      make sure config parameters are printed with yes or no in summary       #
################################################################################

pretty_yes("CAF_FORCE_NO_EXCEPTIONS")

pretty_no("CAF_ENABLE_RUNTIME_CHECKS")
pretty_no("CAF_NO_MEM_MANAGEMENT")
pretty_no("CAF_NO_EXCEPTIONS")
pretty_no("CAF_BUILD_STATIC_ONLY")
pretty_no("CAF_BUILD_STATIC")
pretty_no("CAF_NO_OPENSSL")
pretty_no("CAF_NO_PYTHON")
pretty_no("CAF_NO_TOOLS")
pretty_no("CAF_NO_SUMMARY")

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
# set the library version for our shared library targets
if(CMAKE_HOST_SYSTEM_NAME MATCHES "OpenBSD")
  set(CAF_LIB_VERSION "${CAF_VERSION_MAJOR}.${CAF_VERSION_MINOR}")
else()
  set(CAF_LIB_VERSION "${CAF_VERSION}")
endif()

################################################################################
#   set output paths for binaries and libraries if not provided by the user    #
################################################################################

# prohibit in-source builds
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
    message(FATAL_ERROR "In-source builds are not allowed. Please use "
                        "./configure to choose a build directory and "
                        "initialize the build configuration.")
endif()
# set module path appropriately
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
# set binary output path if not defined by user
if(NOT EXECUTABLE_OUTPUT_PATH)
  set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/bin")
endif()
# set library output path if not defined by user, but always set
# library output path to binary output path for Xcode projects
if(CMAKE_GENERATOR STREQUAL "Xcode")
  set(LIBRARY_OUTPUT_PATH "${EXECUTABLE_OUTPUT_PATH}")
elseif(NOT LIBRARY_OUTPUT_PATH)
  set(LIBRARY_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/lib")
endif()


################################################################################
#                                compiler setup                                #
################################################################################

# set optional build flags
# increase max. template depth on GCC and Clang
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang"
   OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  add_compile_options("-ftemplate-depth=512"
                      "-ftemplate-backtrace-limit=0")
endif()
# explicitly disable obnoxious GCC warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  build_string("EXTRA_FLAGS" "-Wno-missing-field-initializers")
endif()
# add "-Werror" flag if --pedantic-build is used
if(CAF_CXX_WARNINGS_AS_ERRORS)
  build_string("EXTRA_FLAGS" "-Werror")
endif()
# set compiler flags for GCOV if requested
if(CAF_ENABLE_GCOV)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(NO_INLINE "-fno-inline")
  else()
    set(NO_INLINE "-fno-inline -fno-inline-small-functions -fno-default-inline")
  endif()
  # enable a ton of warnings if --more-clang-warnings is used
  if(CAF_MORE_WARNINGS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      set(WFLAGS "-Weverything -Wno-c++98-compat -Wno-padded "
                 "-Wno-documentation-unknown-command -Wno-exit-time-destructors "
                 "-Wno-global-constructors -Wno-missing-prototypes "
                 "-Wno-c++98-compat-pedantic -Wno-unused-member-function "
                 "-Wno-unused-const-variable -Wno-switch-enum "
                 "-Wno-abstract-vbase-init -Wno-shadow "
                 "-Wno-missing-noreturn -Wno-covered-switch-default")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
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
                 "-Wreturn-type -Wsequence-point "
                 "-Wsign-compare -Wswitch -Wtype-limits -Wundef "
                 "-Wuninitialized -Wunused -Wvla -Wwrite-strings")
    endif()
    # convert CMake list to a single string, erasing the ";" separators
    string(REPLACE ";" "" WFLAGS_STR ${WFLAGS})
    build_string("EXTRA_FLAGS" "${WFLAGS_STR}")
  endif()
endif()

# enable address sanitizer if requested by the user
if(CAF_ENABLE_ADDRESS_SANITIZER AND NOT WIN32)
  add_compile_options("-fsanitize=address"
                      "-fno-omit-frame-pointer")
  list(APPEND CAF_EXTRA_LDFLAGS "-fsanitize=address")
endif()
# -pthread is ignored on MacOSX but required on other platforms
if(NOT APPLE AND NOT WIN32)
    build_string("EXTRA_FLAGS" "-pthread")
endif()

# Make sure to link to platform-specific threading dependency (e.g. pthread).
if (NOT TARGET Threads::Threads)
  find_package(Threads REQUIRED)
endif ()
list(APPEND CAF_EXTRA_LDFLAGS Threads::Threads)

# Have CMake set -fPIC when necessary.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Adjust setup for MinGW and Cygwin.
if(MINGW)
  add_definitions(-D_WIN32_WINNT=0x0600)
  add_definitions(-DWIN32)
  include(GenerateExportHeader)
  list(APPEND CAF_EXTRA_LDFLAGS -lws2_32 -liphlpapi -lpsapi)
  # Build static to avoid runtime dependencies to GCC libraries.
  build_string("EXTRA_FLAGS" "-static")
elseif(CYGWIN)
  build_string("EXTRA_FLAGS" "-U__STRICT_ANSI__")
else()
  build_string("EXTRA_FLAGS" "-fPIC")
endif()
if (WIN32)
  list(APPEND CAF_EXTRA_LDFLAGS ws2_32 iphlpapi)
endif()
# iOS support
if(CAF_OSX_SYSROOT)
  set(CMAKE_OSX_SYSROOT "${CAF_OSX_SYSROOT}")
endif()
if(CAF_IOS_DEPLOYMENT_TARGET)
  if(CAF_OSX_SYSROOT STREQUAL "iphonesimulator")
    build_string("EXTRA_FLAGS"
                 "-mios-simulator-version-min=${CAF_IOS_DEPLOYMENT_TARGET}")
  else()
    build_string("EXTRA_FLAGS"
                 "-miphoneos-version-min=${CAF_IOS_DEPLOYMENT_TARGET}")
  endif()
endif()
# check if the user provided CXXFLAGS, set defaults otherwise
if(NOT CMAKE_CXX_FLAGS)
  set(CMAKE_CXX_FLAGS                   "-Wextra -Wall -pedantic ${EXTRA_FLAGS}")
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
if (DEFINED CMAKE_LD_LIBS)
  list(APPEND ${CMAKE_LD_LIBS})
endif()

################################################################################
#                                setup logging                                 #
################################################################################

# make sure log level is defined and all-uppercase
if(NOT CAF_LOG_LEVEL)
  set(CAF_LOG_LEVEL "QUIET")
else()
  string(TOUPPER "${CAF_LOG_LEVEL}" CAF_LOG_LEVEL)
endif()

set(validLogLevels QUIET ERROR WARNING INFO DEBUG TRACE)
list(FIND validLogLevels "${CAF_LOG_LEVEL}" logLevelIndex)
if(logLevelIndex LESS 0)
  MESSAGE(FATAL_ERROR "Invalid log level: \"${CAF_LOG_LEVEL}\"")
endif()

################################################################################
#                           setup for install target                           #
################################################################################

# install includes from test
install(DIRECTORY libcaf_test/caf/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/caf
        FILES_MATCHING PATTERN "*.hpp")

# process cmake_uninstall.cmake.in
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
               "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
               IMMEDIATE @ONLY)
# add uninstall target if it does not exist yet
if(NOT TARGET uninstall)
  add_custom_target(uninstall)
endif()
add_custom_target(caf_uninstall)
add_custom_command(TARGET caf_uninstall
                   PRE_BUILD
                   COMMAND "${CMAKE_COMMAND}" -P
                   "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
add_dependencies(uninstall caf_uninstall)

################################################################################
#                       set inclue paths for subprojects                       #
################################################################################

# path to caf core & io headers
set(CAF_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/libcaf_core"
    "${CMAKE_CURRENT_SOURCE_DIR}/libcaf_io"
    "${CMAKE_CURRENT_SOURCE_DIR}/libcaf_openssl"
    "${CMAKE_CURRENT_SOURCE_DIR}/libcaf_test")
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
include_directories("${CAF_INCLUDE_DIRS}")


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

# build SSL module if OpenSSL library is available or if a parent project
# defined OPENSSL_LIBRARIES for us
if(DEFINED OPENSSL_LIBRARIES)
  if (NOT OPENSSL_INCLUDE_DIR)
    message(FATAL_ERROR "got OPENSSL_LIBRARIES predefined but no OPENSSL_INCLUDE_DIR")
  endif()
  message(STATUS "use OPENSSL_LIBRARIES provided by parent: ${OPENSSL_LIBRARIES}")
  message(STATUS "use OPENSSL_INCLUDE_DIR provided by parent: ${OPENSSL_INCLUDE_DIR}")
  include_directories(BEFORE ${OPENSSL_INCLUDE_DIR})
  add_optional_caf_lib(openssl)
elseif(NOT CAF_NO_OPENSSL)
  find_package(OpenSSL)
  if(OPENSSL_FOUND)
    # Check OpenSSL version >= 1.0.1
    if (OPENSSL_VERSION VERSION_LESS 1.0.1)
      message(STATUS
              "Disable OpenSSL. Required >= 1.0.1 due to TLSv1.2 support.")
      set(CAF_NO_OPENSSL yes)
    else()
      if(NOT CMAKE_CROSSCOMPILING)
        # Check if openssl headers and library versions match
        set(CMAKE_REQUIRED_LIBRARIES ${OPENSSL_LIBRARIES})
        set(CMAKE_REQUIRED_INCLUDES ${OPENSSL_INCLUDE_DIR})
        check_c_source_runs("
          #include <openssl/opensslv.h>
          #include <openssl/crypto.h>
          int main() {
            if (SSLeay() == OPENSSL_VERSION_NUMBER) {
              return 0;
            }
            return -1;
          }
        " OPENSSL_CORRECT_VERSION_NUMBER)
        if (NOT OPENSSL_CORRECT_VERSION_NUMBER)
          message(FATAL_ERROR
            "OpenSSL library version does not match headers")
        endif()
      endif()
      include_directories(BEFORE ${OPENSSL_INCLUDE_DIR})
      add_optional_caf_lib(openssl)
    endif()
  else(OPENSSL_FOUND)
    set(CAF_NO_OPENSSL yes)
  endif(OPENSSL_FOUND)
endif()

# build Python binding if not being told otherwise
if(NOT CAF_NO_PYTHON AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libcaf_python/CMakeLists.txt")
  add_subdirectory(libcaf_python)
endif()

# build examples if not being told otherwise
add_optional_caf_binaries(examples)

# build tools if not being told otherwise
add_optional_caf_binaries(tools)

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
                        ${CAF_EXTRA_LDFLAGS}
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
    add_test(${test_name} ${caf_test} -r 300 -n -v 5 -s "${suite}" ${ARGN})
  endmacro ()
  list(LENGTH suites num_suites)
  message(STATUS "Found ${num_suites} test suites")
  foreach(suite ${suites})
    make_test("${suite}")
  endforeach ()
endif()


# -- Setup for building manual and API documentation ---------------------------

if(NOT caf_is_subproject)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/release.txt")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/release.txt" CAF_RELEASE)
    string(REGEX REPLACE "\n" "" CAF_RELEASE "${CAF_RELEASE}")
  else()
    set(CAF_RELEASE "${CAF_VERSION}")
  endif()
  message(STATUS "Set release version for all documentation to ${CAF_RELEASE}.")
  add_subdirectory(doc)
endif()

# -- Export important variables to the parent scope ----------------------------

if(caf_is_subproject)
  # Make sure parent projects can find build_config.hpp
  list(APPEND CAF_INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}/libcaf_core")
  set(CAF_VERSION  "${CAF_VERSION}" CACHE INTERNAL "CAF release version")
  set(CAF_LIBRARIES "${CAF_LIBRARIES}" CACHE INTERNAL "Library targets")
  set(CAF_INCLUDE_DIRS "${CAF_INCLUDE_DIRS}" CACHE INTERNAL "Path to CAF headers")
endif()

################################################################################
#                                print summary                                 #
################################################################################

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
invertYesNo(CAF_NO_OPENSSL CAF_BUILD_OPENSSL)
invertYesNo(CAF_NO_PYTHON CAF_BUILD_PYTHON)
# collect all compiler flags
string(TOUPPER "${CMAKE_BUILD_TYPE}" UPPER_BUILD_TYPE)
set(ALL_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${UPPER_BUILD_TYPE}}")
set(ALL_LD_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${CAF_EXTRA_LDFLAGS}")
string(STRIP "${ALL_LD_FLAGS}" ALL_LD_FLAGS)
# done
if(NOT CAF_NO_SUMMARY)
  message(STATUS
        "\n====================|  Build Summary  |===================="
        "\n"
        "\nCAF version:           ${CAF_VERSION}"
        "\n"
        "\nBuild type:            ${CMAKE_BUILD_TYPE}"
        "\nBuild static:          ${CAF_BUILD_STATIC}"
        "\nBuild static only:     ${CAF_BUILD_STATIC_ONLY}"
        "\nBuild static runtime:  ${CAF_BUILD_STATIC_RUNTIME}"
        "\nRuntime checks:        ${CAF_ENABLE_RUNTIME_CHECKS}"
        "\nLog level:             ${CAF_LOG_LEVEL}"
        "\nWith mem. mgmt.:       ${CAF_BUILD_MEM_MANAGEMENT}"
        "\nWith exceptions:       ${CAF_BUILD_WITH_EXCEPTIONS}"
        "\n"
        "\nBuild I/O module:      ${CAF_BUILD_IO}"
        "\nBuild tools:           ${CAF_BUILD_TOOLS}"
        "\nBuild examples:        ${CAF_BUILD_EXAMPLES}"
        "\nBuild unit tests:      ${CAF_BUILD_UNIT_TESTS}"
        "\nBuild OpenSSL:         ${CAF_BUILD_OPENSSL}"
        "\nBuild Python:          ${CAF_BUILD_PYTHON}"
        "\n"
        "\nCXX:                   ${CMAKE_CXX_COMPILER}"
        "\nCXXFLAGS:              ${ALL_CXX_FLAGS}"
        "\nLINKER_FLAGS (shared)  ${ALL_LD_FLAGS}"
        "\n"
        "\nSource directory:      ${CMAKE_CURRENT_SOURCE_DIR}"
        "\nBuild directory:       ${CMAKE_CURRENT_BINARY_DIR}"
        "\nExecutable path:       ${EXECUTABLE_OUTPUT_PATH}"
        "\nLibrary path:          ${LIBRARY_OUTPUT_PATH}"
        "\nInstall prefix:        ${CMAKE_INSTALL_PREFIX}"
        "\nGenerator:             ${CMAKE_GENERATOR}"
        "\n"
        "\n===========================================================\n")
endif()
