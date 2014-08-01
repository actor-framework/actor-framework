cmake_minimum_required(VERSION 2.8)
project(caf C CXX)


################################################################################
#   make sure all variables are set to "no" if undefined for summary output    #
################################################################################

if(NOT CAF_ENABLE_RUNTIME_CHECKS)
  set(CAF_ENABLE_RUNTIME_CHECKS no)
endif()
if(NOT CAF_NO_MEM_MANAGEMENT)
  set(CAF_NO_MEM_MANAGEMENT no)
endif()
if(NOT CAF_HAS_PROBE_EVENTS)
  set(CAF_HAS_PROBE_EVENTS no)
endif()
if(NOT CAF_BUILD_STATIC_ONLY)
  set(CAF_BUILD_STATIC_ONLY no)
endif()
if(NOT CAF_BUILD_STATIC)
  set(CAF_BUILD_STATIC no)
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
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(FATAL_ERROR "In-source builds are not allowed. Please use "
                        "./configure to choose a build directory and "
                        "initialize the build configuration.")
endif()
# set module path appropriately
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
# set binary output path if not defined by user
if("${EXECUTABLE_OUTPUT_PATH}" STREQUAL "")
  set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/bin")
endif()
# set library output path if not defined by user, but always set
# library output path to binary output path for Xcode projects
if("${CMAKE_GENERATOR}" STREQUAL "Xcode")
  set(LIBRARY_OUTPUT_PATH "${EXECUTABLE_OUTPUT_PATH}")
elseif("${LIBRARY_OUTPUT_PATH}" STREQUAL "")
  set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/lib")
endif()


################################################################################
#                                compiler setup                                #
################################################################################

# check for g++ >= 4.7 or clang++ > = 3.2
try_run(ProgramResult
        CompilationSucceeded
        ${CMAKE_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/get_compiler_version.cpp
        RUN_OUTPUT_VARIABLE CompilerVersion)
if(NOT CompilationSucceeded OR NOT ProgramResult EQUAL 0)
  message(FATAL_ERROR "Cannot determine compiler version")
elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
  if(CompilerVersion VERSION_GREATER 4.6)
    message(STATUS "Found g++ version ${CompilerVersion}")
  else()
    message(FATAL_ERROR "g++ >= 4.7 required (found: ${CompilerVersion})")
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
# check if the user provided CXXFLAGS, set defaults otherwise
if(CMAKE_CXX_FLAGS)
  set(CXXFLAGS_PROVIDED true)
  set(CMAKE_CXX_FLAGS_DEBUG          "")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "")
  set(CMAKE_CXX_FLAGS_RELEASE        "")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "")
else()
  set(CXXFLAGS_PROVIDED false)
  if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    message(STATUS "NOTE: Automatically added -stdlib=libc++ flag, "
                   "you can override this by defining CMAKE_CXX_FLAGS "
                   "(see 'configure --help')")
    set(CMAKE_CXX_FLAGS "-std=c++11 -stdlib=libc++")
    if(MORE_CLANG_WARNINGS)
      set(CMAKE_CXX_FLAGS "-pedantic -Weverything -Wno-c++98-compat "
                          "-Wno-padded -Wno-documentation-unknown-command "
                          "-Wno-exit-time-destructors -Wno-global-constructors "
                          "-Wno-missing-prototypes -Wno-c++98-compat-pedantic "
                          "-Wno-unused-member-function "
                          "-Wno-unused-const-variable")
    endif()
  else()
    set(CMAKE_CXX_FLAGS "-std=c++11 -Wextra -Wall -pedantic")
  endif()
  set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os")
  set(CMAKE_CXX_FLAGS_RELEASE        "-O3 -DNDEBUG")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")
endif()
# set build default build type to RelWithDebInfo if not set
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
# enable clang's address sanitizer if requested by the user
if(ENABLE_ADDRESS_SANITIZER)
  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
  message(STATUS "Enable address sanitizer")
endif(ENABLE_ADDRESS_SANITIZER)
# -pthread is ignored on MacOSX, enable it for all other platforms
if(NOT APPLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()
# extra setup steps needed on MinGW
if(MINGW)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWIN32")
  include(GenerateExportHeader)
  set(LD_FLAGS "ws2_32 -liphlpapi")
endif()
# needed by subprojects
set(LD_FLAGS ${CMAKE_LD_LIBS})


################################################################################
#            add custom definitions requested via configure script             #
################################################################################

if(CAF_LOG_LEVEL)
  add_definitions(-DCAF_LOG_LEVEL=${CAF_LOG_LEVEL})
endif()

if(CAF_ENABLE_RUNTIME_CHECKS)
  add_definitions(-DCAF_ENABLE_RUNTIME_CHECKS)
endif()

if(CAF_NO_MEM_MANAGEMENT)
  add_definitions(-DCAF_NO_MEM_MANAGEMENT)
endif()


################################################################################
#                           setup for install target                           #
################################################################################

# install includes from core
install(DIRECTORY libcaf_core/caf/
        DESTINATION include/caf FILES_MATCHING PATTERN "*.hpp")
# install CPPA compatibility headers
install(DIRECTORY libcaf_core/cppa/
        DESTINATION include/cppa FILES_MATCHING PATTERN "*.hpp")
# install includes from io
install(DIRECTORY libcaf_io/caf/ DESTINATION include/caf
        FILES_MATCHING PATTERN "*.hpp")
# process cmake_uninstall.cmake.in
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
               "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
               IMMEDIATE @ONLY)
# add uninstall target
add_custom_target(uninstall
                  COMMAND ${CMAKE_COMMAND} -P
                  ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)


################################################################################
#                       set inclue paths for subprojects                       #
################################################################################

# path to caf core & io headers
set(LIBCAF_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/libcaf_core
    ${CMAKE_SOURCE_DIR}/libcaf_io)
# path to probe-event headers (if submodule is loaded)
if(EXISTS "${CMAKE_SOURCE_DIR}/probe-event/caf")
  set(LIBCAF_INCLUDE_DIRS
      ${CMAKE_SOURCE_DIR}/probe-event/ ${LIBCAF_INCLUDE_DIRS})
  # make files from probe-event show up in IDE
  file(GLOB PROBE_EVENTS_HDRS
       "${CMAKE_SOURCE_DIR}/probe-event/caf/probe_event/*.hpp")
  add_custom_target(caf_probe_events_dummy SOURCES ${PROBE_EVENTS_HDRS})
  set(CAF_HAS_PROBE_EVENTS yes)
endif()
# all projects need the headers of the core components
include_directories(${LIBCAF_INCLUDE_DIRS})


################################################################################
#                               add subprojects                                #
################################################################################

# core library
message(STATUS "Enter subdirectory libcaf_core")
add_subdirectory(libcaf_core)
# set core lib for sub directories
set(LIBCAF_CORE_LIBRARY libcaf_core)
message(STATUS "Enter subdirectory libcaf_io")
add_subdirectory(libcaf_io)
# set io lib for sub directories
set(LIBCAF_IO_LIBRARY libcaf_io)
# tell CMake caf_io depends on caf_core
if(CAF_BUILD_STATIC_ONLY OR CAF_BUILD_STATIC)
  add_dependencies(libcaf_ioStatic libcaf_coreStatic)
endif()
if(NOT CAF_BUILD_STATIC_ONLY)
  add_dependencies(libcaf_io libcaf_core)
endif()
# set LIBCAF_LIBRARIES for other subprojects
set(LIBCAF_LIBRARIES ${LIBCAF_CORE_LIBRARY} ${LIBCAF_IO_LIBRARY})
# add unit tests if not being told otherwise
if(NOT CAF_NO_UNIT_TESTS)
  enable_testing()
  message(STATUS "Enter subdirectory unit_testing")
  add_subdirectory(unit_testing)
  add_dependencies(all_unit_tests libcaf_io)
endif()
# build examples if not being told otherwise
if(NOT CAF_NO_EXAMPLES)
  message(STATUS "Enter subdirectory examples")
  add_subdirectory(examples)
  add_dependencies(all_examples libcaf_io)
endif()
# build nexus if not being told otherwise
if(NOT CAF_NO_NEXUS AND EXISTS "${CMAKE_SOURCE_DIR}/nexus/caf/")
  if(NOT CAF_HAS_PROBE_EVENTS)
    message(WARNING "cannot build nexus: probe-event submodule missing")
    set(CAF_NO_NEXUS yes)
  else()
    message(STATUS "Enter subdirectory nexus")
    add_subdirectory(nexus)
    add_dependencies(nexus libcaf_io)
    set(CAF_NO_NEXUS no)
  endif()
else()
  # make sure variable is set for build log
  set(CAF_NO_NEXUS yes)
endif()
# build probe if not being told otherwise
if(NOT CAF_NO_PROBE AND EXISTS "${CMAKE_SOURCE_DIR}/probe/caf/")
  if(NOT CAF_HAS_PROBE_EVENTS)
    message(WARNING "cannot build probe: probe-event submodule missing")
    set(CAF_NO_PROBE yes)
  else()
    message(STATUS "Enter subdirectory probe")
    add_subdirectory(probe)
    if(CAF_BUILD_STATIC_ONLY OR CAF_BUILD_STATIC)
      add_dependencies(libcaf_probeStatic libcaf_ioStatic)
    endif()
    if(NOT CAF_BUILD_STATIC_ONLY)
      add_dependencies(libcaf_probe libcaf_io)
    endif()
    set(CAF_NO_PROBE no)
  endif()
endif()
# build benchmarks if not being told otherwise
if(NOT CAF_NO_BENCHMARKS AND EXISTS "${CMAKE_SOURCE_DIR}/benchmarks/caf/")
  message(STATUS "Enter subdirectory benchmarks")
  add_subdirectory(benchmarks)
  add_dependencies(all_benchmarks libcaf_io)
else()
  # make sure variable is set for build log
  set(CAF_NO_BENCHMARKS yes)
endif()

################################################################################
#                                Doxygen setup                                 #
################################################################################

# check for doxygen and add custom "doc" target to Makefile
find_package(Doxygen)
if(DOXYGEN_FOUND)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
                 @ONLY)
  add_custom_target(doc ${DOXYGEN_EXECUTABLE} ${CMAKE_HOME_DIRECTORY}/Doxyfile
                    WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}
                    COMMENT "Generating API documentation with Doxygen"
                    VERBATIM)
endif(DOXYGEN_FOUND)


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
invertYesNo(CAF_NO_EXAMPLES CAF_BUILD_EXAMPLES)
invertYesNo(CAF_NO_UNIT_TESTS CAF_BUILD_UNIT_TESTS)
invertYesNo(CAF_NO_NEXUS CAF_BUILD_NEXUS)
invertYesNo(CAF_NO_MEM_MANAGEMENT CAF_BUILD_MEM_MANAGEMENT)
invertYesNo(CAF_NO_BENCHMARKS CAF_BUILD_BENCHMARKS)
# collect all compiler flags
string(TOUPPER ${CMAKE_BUILD_TYPE} UPPER_BUILD_TYPE)
set(ALL_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${UPPER_BUILD_TYPE}}")
# done
message(STATUS
        "\n====================|  Build Summary  |===================="
        "\n"
        "\nLibcaf version:    ${CAF_VERSION}"
        "\n"
        "\nBuild type:        ${CMAKE_BUILD_TYPE}"
        "\nBuild static:      ${CAF_BUILD_STATIC}"
        "\nBulid static only: ${CAF_BUILD_STATIC_ONLY}"
        "\nRuntime checks:    ${CAF_ENABLE_RUNTIME_CHECKS}"
        "\nLog level:         ${LOG_LEVEL_STR}"
        "\nWith mem. mgmt.:   ${CAF_BUILD_MEM_MANAGEMENT}"
        "\n"
        "\nBuild examples:    ${CAF_BUILD_EXAMPLES}"
        "\nBuild unit tests:  ${CAF_BUILD_UNIT_TESTS}"
        "\nBuild nexus:       ${CAF_BUILD_NEXUS}"
        "\nBuild probe:       ${CAF_BUILD_PROBE}"
        "\nBuild benchmarks:  ${CAF_BUILD_BENCHMARKS}"
        "\n"
        "\nCXX:               ${CMAKE_CXX_COMPILER}"
        "\nCXXFLAGS:          ${ALL_CXX_FLAGS}"
        "\nLIBRARIES:         ${LD_FLAGS}"
        "\n"
        "\nSource directory:  ${CMAKE_SOURCE_DIR}"
        "\nBuild directory:   ${CMAKE_BINARY_DIR}"
        "\nExecutable path:   ${EXECUTABLE_OUTPUT_PATH}"
        "\nLibrary path:      ${LIBRARY_OUTPUT_PATH}"
        "\nInstall prefix:    ${CMAKE_INSTALL_PREFIX}"
        "\nGenerator:         ${CMAKE_GENERATOR}"
        "\n"
        "\n===========================================================\n")
