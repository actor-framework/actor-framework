cmake_minimum_required(VERSION 2.8)
project(caf C CXX)

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
set(CAF_VERSION "${CAF_VERSION_MAJOR}.${CAF_VERSION_MINOR}.${CAF_VERSION_PATCH}")

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

# check for g++ >= 4.7 or clang++ > = 3.2
try_run(ProgramResult
        CompilationSucceeded
  ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/get_compiler_version.cpp
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
else ()
  message(FATAL_ERROR "Your C++ compiler does not support C++11 "
                      "or is not supported")
endif ()

# check if the user provided CXXFLAGS on the command line
if (CMAKE_CXX_FLAGS)
  set(CXXFLAGS_PROVIDED true)
  set(CMAKE_CXX_FLAGS_DEBUG          "")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "")
  set(CMAKE_CXX_FLAGS_RELEASE        "")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "")
else ()
  set(CXXFLAGS_PROVIDED false)
  if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    message(STATUS "NOTE: Automatically added -stdlib=libc++ flag, "
                   "you can override this by defining CMAKE_CXX_FLAGS "
                   "(see 'configure --help')")
    set(CMAKE_CXX_FLAGS "-std=c++11 -stdlib=libc++")
    if (MORE_CLANG_WARNINGS)
      set(CMAKE_CXX_FLAGS "-pedantic -Weverything -Wno-c++98-compat -Wno-padded -Wno-documentation-unknown-command -Wno-exit-time-destructors -Wno-global-constructors -Wno-missing-prototypes -Wno-c++98-compat-pedantic -Wno-unused-member-function -Wno-unused-const-variable")
    endif ()
  else ()
    set(CMAKE_CXX_FLAGS "-std=c++11 -Wextra -Wall -pedantic")
  endif ()
  if (MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWIN32")
    include (GenerateExportHeader)
  endif(MINGW)
  set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os")
  set(CMAKE_CXX_FLAGS_RELEASE        "-O3 -DNDEBUG")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")
endif ()

# set build type (evaluate CAF_ENABLE_RUNTIME_CHECKS flag)
if (CAF_ENABLE_RUNTIME_CHECKS)
  add_definitions(-DCAF_ENABLE_RUNTIME_CHECKS)
  message(STATUS "Enable runtime checks")
endif (CAF_ENABLE_RUNTIME_CHECKS)

if (ENABLE_ADDRESS_SANITIZER)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
  message(STATUS "Enable address sanitizer")
endif (ENABLE_ADDRESS_SANITIZER)

if (CAF_LOG_LEVEL)
  add_definitions(-DCAF_LOG_LEVEL=${CAF_LOG_LEVEL})
endif(CAF_LOG_LEVEL)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# set build default build type if not set
if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif ("${CMAKE_BUILD_TYPE}" STREQUAL "")

# all libs need access to core headers
set(INCLUDE_DIRS libcaf_core)
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

if (DISABLE_MEM_MANAGEMENT)
  add_definitions(-DCAF_DISABLE_MEM_MANAGEMENT)
endif (DISABLE_MEM_MANAGEMENT)

link_directories(${LD_DIRS})
include_directories(${INCLUDE_DIRS})

# install includes from core
install(DIRECTORY libcaf_core/caf/ DESTINATION include/caf FILES_MATCHING PATTERN "*.hpp")
install(DIRECTORY libcaf_core/cppa/ DESTINATION include/cppa FILES_MATCHING PATTERN "*.hpp")

# install includes from io
install(DIRECTORY libcaf_io/caf/ DESTINATION include/caf FILES_MATCHING PATTERN "*.hpp")

# process cmake_uninstall.cmake.in
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
               "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
               IMMEDIATE @ONLY)

# add uninstall target
add_custom_target(uninstall
                  COMMAND ${CMAKE_COMMAND} -P
                  ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

# set path to caf core & io headers for subdirectories
set (LIBCAF_INCLUDE_DIRS
     ${CMAKE_SOURCE_DIR}/libcaf_core
     ${CMAKE_SOURCE_DIR}/libcaf_io)

# set up subdirectories, todo: allow configure option to disable io
add_subdirectory(libcaf_core)
# set core lib for sub directories
set (LIBCAF_CORE_LIBRARY libcaf_core)
add_subdirectory(libcaf_io)
# set io lib for sub directories
set (LIBCAF_IO_LIBRARY libcaf_io)
if ("${CAF_BUILD_STATIC_ONLY}" STREQUAL "yes" OR "${CAF_BUILD_STATIC}" STREQUAL "yes")
  add_dependencies(libcaf_ioStatic libcaf_coreStatic)
endif ()
if (NOT "${CAF_BUILD_STATIC_ONLY}" STREQUAL "yes")
  add_dependencies(libcaf_io libcaf_core)
endif ()
# set list variable for sub directories
set (LIBCAF_LIBRARIES ${LIBCAF_CORE_LIBRARY} ${LIBCAF_IO_LIBRARY})

if (NOT "${CAF_NO_UNIT_TESTS}" STREQUAL "yes")
  enable_testing()
  add_subdirectory(unit_testing)
  add_dependencies(all_unit_tests libcaf_io)
endif ()
if (NOT "${CAF_NO_EXAMPLES}" STREQUAL "yes")
  add_subdirectory(examples)
  add_dependencies(all_examples libcaf_io)
endif ()

# set optional flags
string(TOUPPER ${CMAKE_BUILD_TYPE} build_type)

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
if (CAF_LOG_LEVEL)
  if (${CAF_LOG_LEVEL} EQUAL 0)
    set(LOG_LEVEL_STR "ERROR")
  elseif (${CAF_LOG_LEVEL} EQUAL 1)
    set(LOG_LEVEL_STR "WARNING")
  elseif (${CAF_LOG_LEVEL} EQUAL 2)
    set(LOG_LEVEL_STR "INFO")
  elseif (${CAF_LOG_LEVEL} EQUAL 3)
    set(LOG_LEVEL_STR "DEBUG")
  elseif (${CAF_LOG_LEVEL} EQUAL 4)
    set(LOG_LEVEL_STR "TRACE")
  else ()
    set(LOG_LEVEL_STR "invalid")
  endif ()
endif (CAF_LOG_LEVEL)

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

toYesNo(CAF_ENABLE_RUNTIME_CHECKS DEBUG_MODE_STR)
toYesNo(ENABLE_OPENCL BUILD_OPENCL_STR)
toYesNo(DISABLE_MEM_MANAGEMENT DISABLE_MEM_MANAGEMENT_STR)
invertYesNo(CAF_NO_EXAMPLES BUILD_EXAMPLES)
invertYesNo(CAF_NO_UNIT_TESTS BUILD_UNIT_TESTS)
invertYesNo(DISABLE_MEM_MANAGEMENT_STR WITH_MEM_MANAGEMENT)

if (NOT "${CAF_BUILD_STATIC}" STREQUAL "yes")
  set(CAF_BUILD_STATIC "no")
endif ()

if (NOT "${CAF_BUILD_STATIC_ONLY}" STREQUAL "yes")
  set(CAF_BUILD_STATIC_ONLY "no")
else ()
  set(CAF_BUILD_STATIC "yes")
endif ()

# done (print summary)
message(STATUS
        "\n====================|  Build Summary  |===================="
        "\n"
        "\nLibcaf version:    ${CAF_VERSION}"
        "\n"
        "\nBuild type:        ${CMAKE_BUILD_TYPE}"
        "\nRuntime checks:    ${DEBUG_MODE_STR}"
        "\nLog level:         ${LOG_LEVEL_STR}"
        "\nBuild examples:    ${BUILD_EXAMPLES}"
        "\nBuild unit tests:  ${BUILD_UNIT_TESTS}"
        "\nBuild static:      ${CAF_BUILD_STATIC}"
        "\nBulid static only: ${CAF_BUILD_STATIC_ONLY}"
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
        "\nGenerator:         ${CMAKE_GENERATOR}"
        "\n"
        "\n===========================================================\n")
