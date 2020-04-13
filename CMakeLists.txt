cmake_minimum_required(VERSION 3.2.3 FATAL_ERROR)
project(CAF CXX)

# -- includes ------------------------------------------------------------------

include(CMakeDependentOption)      # Conditional default values
include(CMakePackageConfigHelpers) # For creating .cmake files
include(GNUInstallDirs)            # Sets default install paths
include(GenerateExportHeader)      # Auto-generates dllexport macros

# -- general options -----------------------------------------------------------

option(BUILD_SHARED_LIBS "Shared library targets" ON)
option(CMAKE_EXPORT_COMPILE_COMMANDS "JSON compile commands database" ON)
option(THREADS_PREFER_PTHREAD_FLAG "Prefer -pthread flag if available " ON)

# -- CAF options that are off by default ----------------------------------------

option(CAF_BUILD_CURL_EXAMPLES "Build examples with libcurl" OFF)
option(CAF_BUILD_PROTOBUF_EXAMPLES "Build examples with Google Protobuf" OFF)
option(CAF_BUILD_QT5_EXAMPLES "Build examples with the Qt5 framework" OFF)
option(CAF_ENABLE_RUNTIME_CHECKS "Build CAF with extra runtime assertions" OFF)
option(CAF_ENABLE_UTILITY_TARGETS "Include targets like consistency-check" OFF)

# -- CAF options that are on by default ----------------------------------------

option(CAF_BUILD_EXAMPLES "Build small programs showcasing CAF features" ON)
option(CAF_BUILD_IO_MODULE "Build networking I/O module" ON)
option(CAF_BUILD_TESTING "Build unit test suites" ON)
option(CAF_BUILD_TOOLS "Build utility programs such as caf-run" ON)
option(CAF_BUILD_WITH_EXCEPTIONS "Build CAF with support for exceptions" ON)

# -- CAF options that depend on others -----------------------------------------

cmake_dependent_option(CAF_BUILD_OPENSSL_MODULE "CAF networking I/O module" ON
                       "CAF_BUILD_IO_MODULE" OFF)

# -- CAF options with non-boolean values ---------------------------------------

set(CAF_LOG_LEVEL "QUIET" CACHE STRING "Max. log verbosity of CAF components")
set(CAF_SANITIZERS "" CACHE STRING
    "Comma separated sanitizers, e.g., 'address,undefined'")
set(CAF_INSTALL_CMAKEDIR
    "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/cmake/CAF" CACHE STRING
    "Path for installing CMake files, enables 'find_package(CAF)'")

# -- macOS-specific options ----------------------------------------------------

if(APPLE)
  option(CMAKE_MACOSX_RPATH "Use rpaths on macOS and iOS" ON)
endif()

# -- project-specific CMake settings -------------------------------------------

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# -- sanity checking -----------------------------------------------------------

if(CAF_BUILD_OPENSSL_MODULE AND NOT CAF_BUILD_IO_MODULE)
  message(FATAL_ERROR "Invalid options: cannot build OpenSSL without I/O")
endif()

# TODO: use `if (CAF_LOG_LEVEL IN_LIST ...)` when switching to CMake ≥ 3.3
if(NOT ";QUIET;ERROR;WARNING;INFO;DEBUG;TRACE;" MATCHES ";${CAF_LOG_LEVEL};")
  message(FATAL_ERROR "Invalid log level: \"${CAF_LOG_LEVEL}\"")
endif()

# -- compiler setup ------------------------------------------------------------

if(MSVC)
  # disable 4275 and 4251 (warnings regarding C++ classes at ABI boundaries)
  add_compile_options(caf-project INTERFACE /wd4275 /wd4251)
  if(CAF_SANITIZERS)
    message(FATAL_ERROR "sanitizer builds are currently not supported on MSVC")
  endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang"
       OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  # Flags for both compilers.
  add_compile_options(-ftemplate-depth=512 -ftemplate-backtrace-limit=0
                      -Wall -Wextra -pedantic)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Flags for Clang only.
    add_compile_options(-Wdocumentation)
  else()
    # Flags for GCC only.
    add_compile_options(-Wno-missing-field-initializers)
  endif()
endif()

if(CAF_SANITIZERS)
  add_compile_options(-fsanitize=${CAF_SANITIZERS} -fno-omit-frame-pointer)
endif()

# -- unit testing setup / caf_add_test_suites function  ------------------------

if(CAF_BUILD_TESTING)
  enable_testing()
  function(caf_add_test_suites target)
    foreach(suiteName ${ARGN})
      string(REPLACE "." "/" suitePath ${suiteName})
      target_sources(${target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/test/${suitePath}.cpp")
      add_test(NAME ${suiteName}
               COMMAND ${target} -r300 -n -v5 -s"^${suiteName}$")
    endforeach()
  endfunction()
endif()

# -- make sure we have at least C++17 available --------------------------------

# TODO: simply set CXX_STANDARD when switching to CMake ≥ 3.9.6
if(NOT CMAKE_CROSSCOMPILING)
  try_compile(caf_has_cxx_17
              "${CMAKE_CURRENT_BINARY_DIR}"
              "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check-compiler-features.cpp")
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
      message(FATAL_ERROR "\nFatal error: unable activate C++17 mode!\
                           \nPlease see README.md for supported compilers.\
                           \n\ntry_compile output:\n${cxx_check_output}")
    endif()
    add_compile_options("${cxx_flag}")
  endif()
endif()

# -- set default visibility to hidden when building shared libs ----------------

if(BUILD_SHARED_LIBS)
  set(CMAKE_CXX_VISIBILITY_PRESET hidden)
  set(CMAKE_VISIBILITY_INLINES_HIDDEN yes)
  if(POLICY CMP0063)
    cmake_policy(SET CMP0063 NEW)
  endif()
endif()

# -- utility targets -----------------------------------------------------------

if(CAF_ENABLE_UTILITY_TARGETS)
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
else()
  function(add_enum_consistency_check hpp_file cpp_file)
    # nop
  endfunction()
endif()

# -- get CAF version -----------------------------------------------------------

# get line containing the version from config.hpp and extract version number
file(READ "libcaf_core/caf/config.hpp" CAF_CONFIG_HPP)
string(REGEX MATCH "#define CAF_VERSION [0-9]+" CAF_VERSION_LINE "${CAF_CONFIG_HPP}")
string(REGEX MATCH "[0-9]+" CAF_VERSION_INT "${CAF_VERSION_LINE}")
# calculate major, minor, and patch version
math(EXPR CAF_VERSION_MAJOR "${CAF_VERSION_INT} / 10000")
math(EXPR CAF_VERSION_MINOR "( ${CAF_VERSION_INT} / 100) % 100")
math(EXPR CAF_VERSION_PATCH "${CAF_VERSION_INT} % 100")
# create full version string
set(CAF_VERSION "${CAF_VERSION_MAJOR}.${CAF_VERSION_MINOR}.${CAF_VERSION_PATCH}")
# set the library version for our shared library targets
if(CMAKE_HOST_SYSTEM_NAME MATCHES "OpenBSD")
  set(CAF_LIB_VERSION "${CAF_VERSION_MAJOR}.${CAF_VERSION_MINOR}")
else()
  set(CAF_LIB_VERSION "${CAF_VERSION}")
endif()

# -- generate build config header ----------------------------------------------

configure_file("${PROJECT_SOURCE_DIR}/cmake/build_config.hpp.in"
               "${CMAKE_BINARY_DIR}/caf/detail/build_config.hpp"
               @ONLY)

# -- install testing DSL headers -----------------------------------------------

# install includes from test
install(DIRECTORY libcaf_test/caf/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/caf
        FILES_MATCHING PATTERN "*.hpp")

add_library(libcaf_test INTERFACE)

target_include_directories(libcaf_test INTERFACE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/libcaf_test>)

add_library(CAF::test ALIAS libcaf_test)

# -- provide an uinstall target ------------------------------------------------

# process cmake_uninstall.cmake.in
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
               "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
               IMMEDIATE @ONLY)

# add uninstall target if it does not exist yet
if(NOT TARGET uninstall)
  add_custom_target(uninstall)
endif()

add_custom_command(TARGET uninstall
                   PRE_BUILD
                   COMMAND "${CMAKE_COMMAND}" -P
                   "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")

# -- build all components the user asked for -----------------------------------

add_subdirectory(libcaf_core)

if(CAF_BUILD_IO_MODULE)
  add_subdirectory(libcaf_io)
endif()

if(CAF_BUILD_OPENSSL_MODULE)
  add_subdirectory(libcaf_openssl)
endif()

if(CAF_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(CAF_BUILD_TOOLS)
  add_subdirectory(tools)
endif()

# -- generate and install .cmake files -----------------------------------------

export(EXPORT CAFTargets FILE CAFTargets.cmake NAMESPACE CAF::)

install(EXPORT CAFTargets
        DESTINATION "${CAF_INSTALL_CMAKEDIR}"
        NAMESPACE CAF::)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/CAFConfigVersion.cmake"
  VERSION ${CAF_VERSION}
  COMPATIBILITY ExactVersion)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CAFConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/CAFConfig.cmake"
  INSTALL_DESTINATION "${CAF_INSTALL_CMAKEDIR}")

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/CAFConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/CAFConfigVersion.cmake"
  DESTINATION
    "${CAF_INSTALL_CMAKEDIR}")
