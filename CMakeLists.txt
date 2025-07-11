cmake_minimum_required(VERSION 3.13..3.27 FATAL_ERROR)
project(CAF VERSION 1.1.0 LANGUAGES CXX)

# -- version boilerplate -------------------------------------------------------

# Individual components of the version number.
set(CAF_VERSION_MAJOR ${PROJECT_VERSION_MAJOR}
    CACHE INTERNAL "The major version of CAF" FORCE)
set(CAF_VERSION_MINOR ${PROJECT_VERSION_MINOR}
    CACHE INTERNAL "The minor version of CAF" FORCE)
set(CAF_VERSION_PATCH ${PROJECT_VERSION_PATCH}
    CACHE INTERNAL "The patch version of CAF" FORCE)

# The full version number as human-readable string.
set(CAF_VERSION "${CAF_VERSION_MAJOR}.${CAF_VERSION_MINOR}.${CAF_VERSION_PATCH}"
    CACHE INTERNAL "The full CAF version string" FORCE)

# The full version number as integer.
if(CAF_VERSION_MAJOR LESS 1)
  set(CAF_VERSION_INT ${CAF_VERSION_MAJOR})
else()
  set(CAF_VERSION_INT "")
endif()
if(CAF_VERSION_MINOR LESS 10)
  set(CAF_VERSION_INT ${CAF_VERSION_INT}0${CAF_VERSION_MINOR})
else()
  set(CAF_VERSION_INT ${CAF_VERSION_INT}${CAF_VERSION_MINOR})
endif()
if(CAF_VERSION_PATCH LESS 10)
  set(CAF_VERSION_INT ${CAF_VERSION_INT}0${CAF_VERSION_PATCH})
else()
  set(CAF_VERSION_INT ${CAF_VERSION_INT}${CAF_VERSION_PATCH})
endif()

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

option(CAF_ENABLE_CPACK "Enable packaging via CPack" OFF)
option(CAF_ENABLE_CURL_EXAMPLES "Build examples with libcurl" OFF)
option(CAF_ENABLE_PROTOBUF_EXAMPLES "Build examples with Google Protobuf" OFF)
option(CAF_ENABLE_QT6_EXAMPLES "Build examples with the Qt6 framework" OFF)
option(CAF_ENABLE_ROBOT_TESTS "Add the Robot tests to CTest " OFF)
option(CAF_ENABLE_RUNTIME_CHECKS "Build CAF with extra runtime assertions" OFF)
option(CAF_USE_STD_FORMAT "Enable std::format support" OFF)

# -- CAF options that are on by default ----------------------------------------

option(CAF_ENABLE_EXAMPLES "Build small programs showcasing CAF features" ON)
option(CAF_ENABLE_EXCEPTIONS "Build CAF with support for exceptions" ON)
option(CAF_ENABLE_IO_MODULE "Build legacy networking I/O module" ON)
option(CAF_ENABLE_NET_MODULE "Build networking I/O module" ON)
option(CAF_ENABLE_TESTING "Build unit test suites" ON)

# -- CAF options that depend on others -----------------------------------------

cmake_dependent_option(CAF_ENABLE_OPENSSL_MODULE "Build OpenSSL module" ON
                       "CAF_ENABLE_IO_MODULE" OFF)

# -- CAF options with non-boolean values ---------------------------------------

set(CAF_CXX_VERSION 17 CACHE STRING "Set the C++ version to use for CAF")
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

# -- doxygen setup -------------------------------------------------------------

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Doxyfile.in"
               "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
               IMMEDIATE @ONLY)

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
  message(STATUS "Build CAF with C++${CAF_CXX_VERSION}")
  add_library(caf_internal INTERFACE)
  target_compile_features(caf_internal INTERFACE cxx_std_${CAF_CXX_VERSION})
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

# -- unit testing setup --------------------------------------------------------

if(CAF_ENABLE_TESTING)
  enable_testing()
endif()

# -- export internal target (may be useful for re-using compiler flags) --------

set_target_properties(caf_internal PROPERTIES EXPORT_NAME internal)

add_library(CAF::internal ALIAS caf_internal)

install(TARGETS caf_internal EXPORT CAFTargets)

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
  generate_export_header(
    libcaf_${component}
    EXPORT_MACRO_NAME ${export_macro_name}
    EXPORT_FILE_NAME "caf/detail/${component}_export.hpp")
  set_target_properties(libcaf_${component} PROPERTIES
                        EXPORT_NAME ${component}
                        SOVERSION ${CAF_VERSION}
                        VERSION ${CAF_VERSION_MAJOR}
                        OUTPUT_NAME caf_${component})
  install(TARGETS libcaf_${component}
          EXPORT CAFTargets
          ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT ${component}
          RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${component}
          LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT ${component})
  install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/caf"
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
          COMPONENT ${component}
          FILES_MATCHING PATTERN "*.hpp"
          PATTERN "*/internal/*" EXCLUDE)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/caf/detail/${component}_export.hpp"
          DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/caf/detail/")
endfunction()

# -- convenience function for automating our component setup -------------------

function(caf_add_log_component name)
  set(CAF_COMPONENT_NAME ${name})
  configure_file("${PROJECT_SOURCE_DIR}/cmake/log.hpp.in"
                 "${CMAKE_CURRENT_BINARY_DIR}/caf/log/${name}.hpp"
                 @ONLY)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/caf/log/${name}.hpp"
          DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/caf/log/")
endfunction()

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
# )
function(caf_add_component name)
  set(varargs DEPENDENCIES HEADERS SOURCES ENUM_TYPES)
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
  set(lib_target "libcaf_${name}")
  if(NOT TARGET ${lib_target})
    add_library(${lib_target} "${PROJECT_SOURCE_DIR}/cmake/dummy.cpp")
  endif()
  set_property(TARGET ${lib_target} PROPERTY POSITION_INDEPENDENT_CODE ON)
  target_link_libraries(${lib_target} ${CAF_ADD_COMPONENT_DEPENDENCIES})
  target_compile_definitions(${lib_target} PRIVATE "libcaf_${name}_EXPORTS")
  target_include_directories(${lib_target} PUBLIC
                             $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                             $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                             $<INSTALL_INTERFACE:include>)
  caf_add_log_component(${name})
  if(BUILD_SHARED_LIBS)
    set_target_properties(${lib_target} PROPERTIES
                          CXX_VISIBILITY_PRESET hidden
                          VISIBILITY_INLINES_HIDDEN ON)
  endif()
  if(CAF_ENABLE_TESTING)
    add_executable("caf-${name}-test" ${CMAKE_CURRENT_SOURCE_DIR}/main.test.cpp)
    target_compile_definitions(
      "caf-${name}-test" PRIVATE
      CAF_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    target_link_libraries("caf-${name}-test" PRIVATE libcaf_test
                          ${CAF_ADD_COMPONENT_DEPENDENCIES} ${lib_target})
  endif()
  foreach(source_file ${CAF_ADD_COMPONENT_SOURCES})
    get_filename_component(ext ${source_file} EXT)
    if(ext STREQUAL ".cpp")
      target_sources(${lib_target} PRIVATE ${source_file})
    elseif(ext STREQUAL ".test.cpp" AND CAF_ENABLE_TESTING)
      get_filename_component(test_name ${source_file} NAME_WE)
      get_filename_component(test_path ${source_file} DIRECTORY)
      string(REPLACE "/" "." test_name "${test_path}/${test_name}")
      target_sources("caf-${name}-test" PRIVATE ${source_file})
      set_property(SOURCE ${source_file} PROPERTY COMPILE_DEFINITIONS
                   CAF_TEST_SUITE_NAME=${test_name})
      add_test(NAME ${test_name} COMMAND "caf-${name}-test" -s "^${test_name}$" -n -v5)
      set_tests_properties(${test_name} PROPERTIES TIMEOUT 300)
      if (MSVC)
        set_source_files_properties(${source_file} PROPERTIES
                                    COMPILE_FLAGS /wd4996)
      else()
        set_source_files_properties(${source_file} PROPERTIES
                                    COMPILE_FLAGS -Wno-deprecated-declarations)
      endif()
    endif()
  endforeach()
  target_link_libraries(${lib_target} ${CAF_ADD_COMPONENT_DEPENDENCIES})
  if(CAF_ADD_COMPONENT_ENUM_TYPES)
    # OBJECT lib: workaround for https://gitlab.kitware.com/cmake/cmake/-/issues/18399
    add_library(${name}_enums OBJECT)
    set_property(TARGET ${name}_enums PROPERTY POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(${name}_enums CAF::internal)
    if(NOT name STREQUAL core)
      # Implementations need access to headers such as config.hpp.
      target_link_libraries(${name}_enums CAF::core)
    endif()
    target_include_directories(${name}_enums PRIVATE
                               ${CMAKE_CURRENT_SOURCE_DIR}
                               ${CMAKE_CURRENT_BINARY_DIR})
    foreach(enum_name ${CAF_ADD_COMPONENT_ENUM_TYPES})
      caf_add_enum_type(${name}_enums ${enum_name})
    endforeach()
    target_sources(${lib_target} PRIVATE $<TARGET_OBJECTS:${name}_enums>)
    target_compile_definitions(${name}_enums PRIVATE libcaf_${name}_EXPORTS)
  endif()
  caf_export_and_install_lib(${name})
endfunction()

# -- convenience function for adding executable targets of tests ---------------

# Usage:
# caf_add_test_executable(
#   foo
#   DEPENDENCIES
#     ...
#   SOURCES
#     ...
# )
function(caf_add_test_executable name)
  if(NOT CAF_ENABLE_TESTING)
    return()
  endif()
  set(varargs DEPENDENCIES SOURCES)
  cmake_parse_arguments(args "" "" "${varargs}" ${ARGN})
  if(NOT args_SOURCES)
    message(FATAL_ERROR "Cannot add a CAF test executable without sources.")
  endif()
  add_executable(${name} ${args_SOURCES})
  target_link_libraries(${name} PRIVATE CAF::internal ${args_DEPENDENCIES})
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
  COMPATIBILITY SameMajorVersion)

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
