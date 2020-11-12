cmake_minimum_required(VERSION 3.4 FATAL_ERROR)
project(CAF CXX)

# -- includes ------------------------------------------------------------------

include(CMakeDependentOption)      # Conditional default values
include(CMakePackageConfigHelpers) # For creating .cmake files
include(CheckCXXSourceCompiles)    # For checking whether some C++ features work
include(GNUInstallDirs)            # Sets default install paths
include(GenerateExportHeader)      # Auto-generates dllexport macros

# -- override CMake defaults for internal cache entries ------------------------

set(CMAKE_EXPORT_COMPILE_COMMANDS ON
    CACHE INTERNAL "Write JSON compile commands database")

# -- general options -----------------------------------------------------------

option(BUILD_SHARED_LIBS "Build shared library targets" ON)
option(THREADS_PREFER_PTHREAD_FLAG "Prefer -pthread flag if available " ON)

# -- CAF options that are off by default ---------------------------------------

option(CAF_ENABLE_CURL_EXAMPLES "Build examples with libcurl" OFF)
option(CAF_ENABLE_PROTOBUF_EXAMPLES "Build examples with Google Protobuf" OFF)
option(CAF_ENABLE_QT5_EXAMPLES "Build examples with the Qt5 framework" OFF)
option(CAF_ENABLE_RUNTIME_CHECKS "Build CAF with extra runtime assertions" OFF)
option(CAF_ENABLE_UTILITY_TARGETS "Include targets like consistency-check" OFF)
option(CAF_ENABLE_ACTOR_PROFILER "Enable experimental profiler API" OFF)

# -- CAF options that are on by default ----------------------------------------

option(CAF_ENABLE_EXAMPLES "Build small programs showcasing CAF features" ON)
option(CAF_ENABLE_IO_MODULE "Build networking I/O module" ON)
option(CAF_ENABLE_TESTING "Build unit test suites" ON)
option(CAF_ENABLE_TOOLS "Build utility programs such as caf-run" ON)
option(CAF_ENABLE_EXCEPTIONS "Build CAF with support for exceptions" ON)

# -- CAF options that depend on others -----------------------------------------

cmake_dependent_option(CAF_ENABLE_OPENSSL_MODULE "Build OpenSSL module" ON
                       "CAF_ENABLE_IO_MODULE" OFF)

# -- CAF options with non-boolean values ---------------------------------------

set(CAF_LOG_LEVEL "QUIET" CACHE STRING "Set log verbosity of CAF components")
set(CAF_SANITIZERS "" CACHE STRING
    "Comma separated sanitizers, e.g., 'address,undefined'")
set(CAF_INSTALL_CMAKEDIR
    "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/cmake/CAF" CACHE PATH
    "Path for installing CMake files, enables 'find_package(CAF)'")
  set(CAF_BUILD_INFO_FILE_PATH "" CACHE FILEPATH
    "Optional path for writing CMake and compiler version information")

# -- macOS-specific options ----------------------------------------------------

if(APPLE)
  set(CMAKE_MACOSX_RPATH ON CACHE INTERNAL "Use rpaths on macOS and iOS")
endif()

# -- project-specific CMake settings -------------------------------------------

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# -- sanity checking -----------------------------------------------------------

if(CAF_ENABLE_OPENSSL_MODULE AND NOT CAF_ENABLE_IO_MODULE)
  message(FATAL_ERROR "Invalid options: cannot build OpenSSL without I/O")
endif()

set(CAF_VALID_LOG_LEVELS QUIET ERROR WARNING INFO DEBUG TRACE)
if(NOT CAF_LOG_LEVEL IN_LIST CAF_VALID_LOG_LEVELS)
  message(FATAL_ERROR "Invalid log level: \"${CAF_LOG_LEVEL}\"")
endif()

if(MSVC AND CAF_SANITIZERS)
  message(FATAL_ERROR "Sanitizer builds are currently not supported on MSVC")
endif()

# -- compiler setup ------------------------------------------------------------

# Calls target_link_libraries for non-object library targets.
function(caf_target_link_libraries target access)
  get_target_property(targetType ${target} TYPE)
  if (NOT targetType STREQUAL OBJECT_LIBRARY)
    target_link_libraries(${target} ${access} ${ARGN})
  else()
    # If we can't link against it, at least make sure to pull in INTERFACE
    # includes and compiler options.
    foreach(arg ${ARGN})
      if (TARGET ${arg})
        target_include_directories(
          ${target} PRIVATE
          $<TARGET_PROPERTY:${arg},INTERFACE_INCLUDE_DIRECTORIES>)
        target_compile_options(
          ${target} PRIVATE
          $<TARGET_PROPERTY:${arg},INTERFACE_COMPILE_OPTIONS>)
      endif()
    endforeach()
  endif()
endfunction()

function(caf_set_default_properties)
  foreach(target ${ARGN})
    if(MSVC)
      # Disable 4275 and 4251 (warnings regarding C++ classes at ABI boundaries).
      target_compile_options(${target} PRIVATE /wd4275 /wd4251)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang"
           OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
      # Flags for both compilers.
      target_compile_options(${target} PRIVATE -Wall -Wextra -pedantic
                             -ftemplate-depth=512 -ftemplate-backtrace-limit=0)
      if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Flags for Clang only.
        target_compile_options(${target} PRIVATE -Wdocumentation)
      else()
        # Flags for GCC only.
        target_compile_options(${target} PRIVATE
                               -Wno-missing-field-initializers)
      endif()
    endif()
    if(CAF_SANITIZERS)
      target_compile_options(${target} PRIVATE
                             -fsanitize=${CAF_SANITIZERS}
                             -fno-omit-frame-pointer)
      caf_target_link_libraries(${target} PRIVATE
                                -fsanitize=${CAF_SANITIZERS}
                                -fno-omit-frame-pointer)
    endif()
    # All of our object libraries can end up in shared libs => PIC.
    get_target_property(targetType ${target} TYPE)
    if (targetType STREQUAL OBJECT_LIBRARY)
      set_property(TARGET ${target} PROPERTY POSITION_INDEPENDENT_CODE ON)
    endif()
    # We always place headers in the same directories and need to find generated
    # headers from the bin dir.
    target_include_directories(${target} PRIVATE "${CMAKE_BINARY_DIR}"
                               "${CMAKE_CURRENT_SOURCE_DIR}")
  endforeach()
endfunction()

# -- unit testing setup / caf_add_test_suites function  ------------------------

if(CAF_ENABLE_TESTING)
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
  cmake_policy(SET CMP0063 NEW)
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
  function(caf_add_enum_consistency_check hpp_file cpp_file)
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
  function(caf_add_enum_consistency_check hpp_file cpp_file)
    # nop
  endfunction()
endif()

# -- get CAF version -----------------------------------------------------------

# Get line containing the version from config.hpp and extract version number.
file(READ "libcaf_core/caf/config.hpp" CAF_CONFIG_HPP)
string(REGEX MATCH "#define CAF_VERSION [0-9]+" CAF_VERSION_LINE "${CAF_CONFIG_HPP}")
string(REGEX MATCH "[0-9]+" CAF_VERSION_INT "${CAF_VERSION_LINE}")
# Calculate major, minor, and patch version.
math(EXPR CAF_VERSION_MAJOR "${CAF_VERSION_INT} / 10000")
math(EXPR CAF_VERSION_MINOR "( ${CAF_VERSION_INT} / 100) % 100")
math(EXPR CAF_VERSION_PATCH "${CAF_VERSION_INT} % 100")
# Create full version string.
set(CAF_VERSION "${CAF_VERSION_MAJOR}.${CAF_VERSION_MINOR}.${CAF_VERSION_PATCH}"
  CACHE INTERNAL "The full CAF version string")
# Set the library version for our shared library targets.
if(CMAKE_HOST_SYSTEM_NAME MATCHES "OpenBSD")
  set(CAF_LIB_VERSION "${CAF_VERSION_MAJOR}.${CAF_VERSION_MINOR}"
      CACHE INTERNAL "The version string used for shared library objects")
else()
  set(CAF_LIB_VERSION "${CAF_VERSION}"
      CACHE INTERNAL "The version string used for shared library objects")
endif()

# -- generate build config header ----------------------------------------------

configure_file("${PROJECT_SOURCE_DIR}/cmake/build_config.hpp.in"
               "${CMAKE_BINARY_DIR}/caf/detail/build_config.hpp"
               @ONLY)

# -- install testing DSL headers -----------------------------------------------

add_library(libcaf_test INTERFACE)

set_target_properties(libcaf_test PROPERTIES EXPORT_NAME test)

target_include_directories(libcaf_test INTERFACE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/libcaf_test>)

add_library(CAF::test ALIAS libcaf_test)

install(DIRECTORY libcaf_test/caf/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/caf
        FILES_MATCHING PATTERN "*.hpp")

install(TARGETS libcaf_test
        EXPORT CAFTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT test
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT test
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT test)

# -- add uninstall target if it does not exist yet -----------------------------

if(NOT TARGET uninstall)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/uninstall.cmake.in"
                 "${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake"
                 IMMEDIATE @ONLY)
  add_custom_target(
    uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake)
endif()


# -- utility function for installing library targets ---------------------------

function(caf_export_and_install_lib component)
  add_library(CAF::${component} ALIAS libcaf_${component})
  string(TOUPPER "CAF_${component}_EXPORT" exportMacroName)
  target_include_directories(libcaf_${component} INTERFACE
                             $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                             $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
                             $<INSTALL_INTERFACE:include>)
  generate_export_header(
    libcaf_${component}
    EXPORT_MACRO_NAME ${exportMacroName}
    EXPORT_FILE_NAME "${CMAKE_BINARY_DIR}/caf/detail/${component}_export.hpp")
  set_target_properties(libcaf_${component} PROPERTIES
                        EXPORT_NAME ${component}
                        SOVERSION ${CAF_VERSION}
                        VERSION ${CAF_LIB_VERSION}
                        OUTPUT_NAME caf_${component})
  install(FILES "${CMAKE_BINARY_DIR}/caf/detail/${component}_export.hpp"
          DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/caf/detail")
  install(TARGETS libcaf_${component}
          EXPORT CAFTargets
          ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT ${component}
          RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${component}
          LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT ${component})
  install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/caf"
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
          COMPONENT ${component}
          FILES_MATCHING PATTERN "*.hpp")
endfunction()

# -- build all components the user asked for -----------------------------------

add_subdirectory(libcaf_core)

if(CAF_ENABLE_IO_MODULE)
  add_subdirectory(libcaf_io)
endif()

if(CAF_ENABLE_OPENSSL_MODULE)
  add_subdirectory(libcaf_openssl)
endif()

if(CAF_ENABLE_EXAMPLES)
  add_subdirectory(examples)
endif()

if(CAF_ENABLE_TOOLS)
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

# -- extra file output (primarily for CAF CI) ----------------------------------

if(CAF_BUILD_INFO_FILE_PATH)
  configure_file("${PROJECT_SOURCE_DIR}/cmake/caf-build-info.txt.in"
                 "${CAF_BUILD_INFO_FILE_PATH}"
                 @ONLY)
endif()
