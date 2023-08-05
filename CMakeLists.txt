cmake_minimum_required(VERSION 3.5...3.18 FATAL_ERROR)
project(CAF CXX)

# -- includes ------------------------------------------------------------------

include(CMakeDependentOption)
include(CMakePackageConfigHelpers)
include(CheckCXXSourceCompiles)
include(CheckCXXSourceRuns)
include(GNUInstallDirs)
include(GenerateExportHeader)

# -- override CMake defaults for internal cache entries ------------------------

set(CMAKE_EXPORT_COMPILE_COMMANDS ON
    CACHE INTERNAL "Write JSON compile commands database")

# -- general options -----------------------------------------------------------

option(BUILD_SHARED_LIBS "Build shared library targets" ON)
option(THREADS_PREFER_PTHREAD_FLAG "Prefer -pthread flag if available " ON)

# -- CAF options that are off by default ---------------------------------------

option(CAF_ENABLE_ACTOR_PROFILER "Enable experimental profiler API" OFF)
option(CAF_ENABLE_CPACK "Enable packaging via CPack" OFF)
option(CAF_ENABLE_CURL_EXAMPLES "Build examples with libcurl" OFF)
option(CAF_ENABLE_PROTOBUF_EXAMPLES "Build examples with Google Protobuf" OFF)
option(CAF_ENABLE_QT6_EXAMPLES "Build examples with the Qt6 framework" OFF)
option(CAF_ENABLE_ROBOT_TESTS "Add the Robot tests to CTest " OFF)
option(CAF_ENABLE_RUNTIME_CHECKS "Build CAF with extra runtime assertions" OFF)

# -- CAF options that are on by default ----------------------------------------

option(CAF_ENABLE_EXAMPLES "Build small programs showcasing CAF features" ON)
option(CAF_ENABLE_EXCEPTIONS "Build CAF with support for exceptions" ON)
option(CAF_ENABLE_IO_MODULE "Build legacy networking I/O module" ON)
option(CAF_ENABLE_NET_MODULE "Build networking I/O module" ON)
option(CAF_ENABLE_TESTING "Build unit test suites" ON)
option(CAF_ENABLE_TOOLS "Build utility programs such as caf-run" ON)

# -- CAF options that depend on others -----------------------------------------

cmake_dependent_option(CAF_ENABLE_OPENSSL_MODULE "Build OpenSSL module" ON
                       "CAF_ENABLE_IO_MODULE" OFF)

# -- CAF options with non-boolean values ---------------------------------------

set(CAF_LOG_LEVEL "QUIET" CACHE STRING "Set log verbosity of CAF components")
set(CAF_EXCLUDE_TESTS "" CACHE STRING "List of excluded test suites")
set(CAF_SANITIZERS "" CACHE STRING
    "Comma separated sanitizers, e.g., 'address,undefined'")
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

# -- get dependencies that are used in more than one module --------------------

if(CAF_ENABLE_OPENSSL_MODULE OR CAF_ENABLE_NET_MODULE)
  if(NOT TARGET OpenSSL::SSL OR NOT TARGET OpenSSL::Crypto)
    find_package(OpenSSL REQUIRED)
  endif()
endif()

# -- base target setup ---------------------------------------------------------

# This target propagates compiler flags, extra dependencies, etc. All other CAF
# targets pull this target in as a PRIVATE dependency. Users that embed CAF into
# their own CMake scaffold (e.g., via FetchContent) may pass this target in with
# some properties predefined in order to force compiler flags or dependencies.
if(NOT TARGET caf_internal)
  add_library(caf_internal INTERFACE)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  target_compile_options(caf_internal INTERFACE -Wall -Wextra -pedantic
                         -ftemplate-depth=512 -ftemplate-backtrace-limit=0)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(caf_internal INTERFACE -Wdocumentation)
  else()
    target_compile_options(caf_internal INTERFACE
                           -Wno-missing-field-initializers)
  endif()
elseif(MSVC)
  target_compile_options(caf_internal INTERFACE /EHsc)
endif()

# -- unit testing setup / caf_add_legacy_test_suites function  ------------------------

if(CAF_ENABLE_TESTING)
  enable_testing()
  function(caf_add_legacy_test_suites target)
    foreach(suiteName ${ARGN})
      if(NOT ${suiteName} IN_LIST CAF_EXCLUDE_TESTS)
        string(REPLACE "." "/" suitePath ${suiteName})
        target_sources(${target} PRIVATE
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/legacy/${suitePath}.cpp")
        add_test(NAME ${suiteName}
                 COMMAND ${target} -r300 -n -v5 -s "^${suiteName}$")
      endif()
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
    target_compile_options(caf_internal INTERFACE "${cxx_flag}")
  endif()
endif()

# -- check whether we can use std::format --------------------------------------

if(NOT DEFINED CAF_USE_STD_FORMAT)
  set(CAF_USE_STD_FORMAT OFF CACHE BOOL "Enable std::format support" FORCE)
  if(NOT CMAKE_CROSSCOMPILING)
    set(snippet "#include <format>
                 #include <iostream>
                 int main() { std::cout << std::format(\"{}\", \"ok\"); }")
    check_cxx_source_runs("${snippet}" snippet_output)
    if("${snippet_output}" STREQUAL "ok")
      message(STATUS "Enable std::format support")
      set(CAF_USE_STD_FORMAT ON CACHE BOOL "Enable std::format support" FORCE)
    else()
      message(STATUS "Disable std::format support: NOT available")
    endif()
  endif()
endif()

# -- export internal target (may be useful for re-using compiler flags) --------

set_target_properties(caf_internal PROPERTIES EXPORT_NAME internal)

add_library(CAF::internal ALIAS caf_internal)

install(TARGETS caf_internal EXPORT CAFTargets)

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

# -- create the libcaf_test target ahead of time for caf_core ------------------

add_library(libcaf_test)

# -- add uninstall target if it does not exist yet -----------------------------

if(NOT TARGET uninstall)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/uninstall.cmake.in"
                 "${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake"
                 IMMEDIATE @ONLY)
  add_custom_target(
    uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake)
endif()

# -- utility functions ---------------------------------------------------------

# generates the implementation file for the enum that contains to_string,
# from_string and from_integer
function(caf_add_enum_type target enum_name)
  string(REPLACE "." "/" path "${enum_name}")
  set(hpp_file "${CMAKE_CURRENT_SOURCE_DIR}/caf/${path}.hpp")
  set(cpp_file "${CMAKE_CURRENT_BINARY_DIR}/src/${path}_strings.cpp")
  set(gen_file "${PROJECT_SOURCE_DIR}/cmake/caf-generate-enum-strings.cmake")
  add_custom_command(OUTPUT "${cpp_file}"
                     COMMAND ${CMAKE_COMMAND}
                       "-DINPUT_FILE=${hpp_file}"
                       "-DOUTPUT_FILE=${cpp_file}"
                       -P "${gen_file}"
                     DEPENDS "${hpp_file}" "${gen_file}")
  target_sources(${target} PRIVATE "${cpp_file}")
endfunction()

function(caf_export_and_install_lib component)
  add_library(CAF::${component} ALIAS libcaf_${component})
  string(TOUPPER "CAF_${component}_EXPORT" export_macro_name)
  target_include_directories(libcaf_${component} INTERFACE
                             $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                             $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                             $<INSTALL_INTERFACE:include>)
  generate_export_header(
    libcaf_${component}
    EXPORT_MACRO_NAME ${export_macro_name}
    EXPORT_FILE_NAME "caf/detail/${component}_export.hpp")
  set_target_properties(libcaf_${component} PROPERTIES
                        EXPORT_NAME ${component}
                        SOVERSION ${CAF_VERSION}
                        VERSION ${CAF_LIB_VERSION}
                        OUTPUT_NAME caf_${component})
  install(TARGETS libcaf_${component}
          EXPORT CAFTargets
          ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT ${component}
          RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${component}
          LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT ${component})
  install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/caf"
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
          COMPONENT ${component}
          FILES_MATCHING PATTERN "*.hpp")
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/caf/detail/${component}_export.hpp"
          DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/caf/detail/")
endfunction()

# TODO: remove when switching to CMake > 3.12
function(caf_target_link_libraries target)
  if(CMAKE_VERSION VERSION_LESS 3.12)
    get_target_property(target_type ${target} TYPE)
    if (NOT target_type STREQUAL OBJECT_LIBRARY)
      target_link_libraries(${target} ${ARGN})
    else()
      cmake_parse_arguments(CAF_TARGET_LINK_LIBRARIES "" ""
                            "PUBLIC;PRIVATE;INTERFACE" ${ARGN})
      # If we can't link against it, at least make sure to pull in include paths
      # and compiler options.
      foreach(arg IN LISTS CAF_TARGET_LINK_LIBRARIES_PUBLIC
                           CAF_TARGET_LINK_LIBRARIES_PRIVATE)
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
  else()
    target_link_libraries(${target} ${ARGN})
  endif()
endfunction()

# -- convenience function for automating our component setup -------------------

# Usage:
# caf_add_component(
#   foo
#   DEPENDENCIES
#     INTERFACE
#       ...
#     PUBLIC
#       ...
#     PRIVATE
#       ...
#   HEADERS
#     ...
#   SOURCES
#     ...
#   LEGACY_TEST_SOURCES
#     ...
#   LEGACY_TEST_SUITES
#     ...
# )
function(caf_add_component name)
  set(varargs DEPENDENCIES HEADERS SOURCES ENUM_TYPES LEGACY_TEST_SOURCES 
              LEGACY_TEST_SUITES)
  cmake_parse_arguments(CAF_ADD_COMPONENT "" "" "${varargs}" ${ARGN})
  if(NOT CAF_ADD_COMPONENT_HEADERS)
    message(FATAL_ERROR "Cannot add CAF component without at least one header.")
  endif()
  if(NOT CAF_ADD_COMPONENT_SOURCES)
    message(FATAL_ERROR "Cannot add CAF component without at least one source.")
  endif()
  foreach(param DEPENDENCIES HEADERS SOURCES)
    if(NOT CAF_ADD_COMPONENT_${param})
      message(FATAL_ERROR "caf_add_component(): missing parameter ${param}")
    endif()
  endforeach()
  set(pub_lib_target "libcaf_${name}")
  set(obj_lib_target "libcaf_${name}_obj")
  set(targets ${pub_lib_target} ${obj_lib_target})
  add_library(${obj_lib_target} OBJECT ${CAF_ADD_COMPONENT_HEADERS})
  set_property(TARGET ${obj_lib_target} PROPERTY POSITION_INDEPENDENT_CODE ON)
  caf_target_link_libraries(${obj_lib_target}
                            ${CAF_ADD_COMPONENT_DEPENDENCIES})
  foreach(source_file ${CAF_ADD_COMPONENT_SOURCES})
    get_filename_component(ext ${source_file} EXT)
    if(ext STREQUAL ".cpp")
      target_sources(${obj_lib_target} PRIVATE ${source_file})
    elseif(ext STREQUAL ".test.cpp" AND CAF_ENABLE_TESTING)
      get_filename_component(test_name ${source_file} NAME_WE)
      get_filename_component(test_path ${source_file} DIRECTORY)
      string(REPLACE "/" "-" test_name "${test_path}/${test_name}-test")
      add_executable("${test_name}" ${source_file}
                     $<TARGET_OBJECTS:${obj_lib_target}>)
      target_link_libraries(${test_name} PRIVATE libcaf_test
                            ${CAF_ADD_COMPONENT_DEPENDENCIES})
      target_include_directories(${test_name} PRIVATE
                                 "${CMAKE_CURRENT_SOURCE_DIR}"
                                 "${CMAKE_CURRENT_BINARY_DIR}")
      add_test(NAME ${test_name} COMMAND ${test_name})
    endif()
  endforeach()
  if(NOT TARGET ${pub_lib_target})
    add_library(${pub_lib_target}
                "${PROJECT_SOURCE_DIR}/cmake/dummy.cpp"
                $<TARGET_OBJECTS:${obj_lib_target}>)
  else()
    target_sources(
      ${pub_lib_target}
      PRIVATE
        "${PROJECT_SOURCE_DIR}/cmake/dummy.cpp"
        $<TARGET_OBJECTS:${obj_lib_target}>)
  endif()
  if(CAF_ENABLE_TESTING AND CAF_ADD_COMPONENT_LEGACY_TEST_SOURCES)
    set(tst_bin_target "caf-${name}-legacy-test")
    list(APPEND targets ${tst_bin_target})
    add_executable(${tst_bin_target}
                   ${CAF_ADD_COMPONENT_LEGACY_TEST_SOURCES}
                   $<TARGET_OBJECTS:${obj_lib_target}>)
    target_link_libraries(${tst_bin_target} PRIVATE libcaf_test
                          ${CAF_ADD_COMPONENT_DEPENDENCIES})
    target_include_directories(${tst_bin_target} PRIVATE
                               "${CMAKE_CURRENT_SOURCE_DIR}/tests/legacy")
    if(CAF_ADD_COMPONENT_LEGACY_TEST_SUITES)
      caf_add_legacy_test_suites(${tst_bin_target} ${CAF_ADD_COMPONENT_LEGACY_TEST_SUITES})
    endif()
  endif()
  target_link_libraries(${pub_lib_target} ${CAF_ADD_COMPONENT_DEPENDENCIES})
  if(CAF_ADD_COMPONENT_ENUM_TYPES)
    foreach(enum_name ${CAF_ADD_COMPONENT_ENUM_TYPES})
      if(obj_lib_target)
        caf_add_enum_type(${obj_lib_target} ${enum_name})
      else()
        caf_add_enum_type(${pub_lib_target} ${enum_name})
      endif()
    endforeach()
  endif()
  foreach(target ${targets})
    target_compile_definitions(${target} PRIVATE "libcaf_${name}_EXPORTS")
    target_include_directories(${target} PRIVATE
                               "${CMAKE_CURRENT_SOURCE_DIR}"
                               "${CMAKE_CURRENT_BINARY_DIR}")
    if(BUILD_SHARED_LIBS)
      set_target_properties(${target} PROPERTIES
                            CXX_VISIBILITY_PRESET hidden
                            VISIBILITY_INLINES_HIDDEN ON)
    endif()
  endforeach()
  caf_export_and_install_lib(${name})
endfunction()

# -- build all components the user asked for -----------------------------------

add_subdirectory(libcaf_core)

add_subdirectory(libcaf_test)

if(CAF_ENABLE_NET_MODULE)
  add_subdirectory(libcaf_net)
endif()

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

# -- optionally add the Robot tests to CTest -----------------------------------

if(CAF_ENABLE_TESTING AND CAF_ENABLE_ROBOT_TESTS)
  add_subdirectory(robot)
endif()

# -- add top-level compiler and linker flags that propagate to clients ---------

# Disable warnings regarding C++ classes at ABI boundaries on MSVC.
if(BUILD_SHARED_LIBS AND MSVC)
  target_compile_options(libcaf_core INTERFACE /wd4275 /wd4251)
endif()

# Propgatate sanitizer flags to downstream targets.
if(CAF_SANITIZERS)
  foreach(target caf_internal libcaf_core)
    target_compile_options(${target} INTERFACE
                           -fsanitize=${CAF_SANITIZERS}
                           -fno-omit-frame-pointer)
    if(CMAKE_VERSION VERSION_LESS 3.13)
      target_link_libraries(${target} INTERFACE
                            -fsanitize=${CAF_SANITIZERS}
                            -fno-omit-frame-pointer)
    else()
      target_link_options(${target} INTERFACE
                          -fsanitize=${CAF_SANITIZERS}
                          -fno-omit-frame-pointer)
    endif()
  endforeach()
endif()


# -- generate and install .cmake files -----------------------------------------

export(EXPORT CAFTargets FILE CAFTargets.cmake NAMESPACE CAF::)

install(EXPORT CAFTargets
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/CAF"
        NAMESPACE CAF::)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/CAFConfigVersion.cmake"
  VERSION ${CAF_VERSION}
  COMPATIBILITY ExactVersion)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CAFConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/CAFConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/CAF")

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/CAFConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/CAFConfigVersion.cmake"
  DESTINATION
    "${CMAKE_INSTALL_LIBDIR}/cmake/CAF")

# -- extra file output (primarily for CAF CI) ----------------------------------

if(CAF_BUILD_INFO_FILE_PATH)
  configure_file("${PROJECT_SOURCE_DIR}/cmake/caf-build-info.txt.in"
                 "${CAF_BUILD_INFO_FILE_PATH}"
                 @ONLY)
endif()

# -- CPack setup ---------------------------------------------------------------

if(CAF_ENABLE_CPACK)
  include(InstallRequiredSystemLibraries)
  set(CPACK_PACKAGE_NAME "CAF")
  set(CPACK_PACKAGE_VENDOR "CAF Community")
  set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
  set(CPACK_PACKAGE_VERSION_MAJOR "${CAF_VERSION_MAJOR}")
  set(CPACK_PACKAGE_VERSION_MINOR "${CAF_VERSION_MINOR}")
  set(CPACK_PACKAGE_VERSION_PATCH "${CAF_VERSION_PATCH}")
  set(CPACK_PACKAGE_DESCRIPTION
      "To learn more about CAF, please visit https://www.actor-framework.org.")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
        "CAF offers high-level building blocks for concurrency & distribution.")
  set(CPACK_PACKAGE_HOMEPAGE_URL "https://www.actor-framework.org")
  set(CPACK_PACKAGE_ICON "${PROJECT_SOURCE_DIR}/doc/png/logo-1000x1000.png")
  if(NOT CPACK_SOURCE_GENERATOR)
    set(CPACK_SOURCE_GENERATOR "TGZ")
  endif()
  include(CPack)
endif()
