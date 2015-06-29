cmake_minimum_required(VERSION 2.8)
project(caf C CXX)

# silence policy CMP0042 warning by enabling RPATH explicitly
if(APPLE AND NOT DEFINED CMAKE_MACOSX_RPATH)
  set(CMAKE_MACOSX_RPATH true)
endif()

################################################################################
#   make sure all variables are set to "no" if undefined for summary output    #
################################################################################

if(NOT CAF_ENABLE_RUNTIME_CHECKS)
  set(CAF_ENABLE_RUNTIME_CHECKS no)
endif()
if(NOT CAF_NO_MEM_MANAGEMENT)
  set(CAF_NO_MEM_MANAGEMENT no)
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
if(NOT CAF_NO_SUMMARY)
  set(CAF_NO_SUMMARY no)
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

# check for g++ >= 4.7 or clang++ > = 3.2
if(NOT WIN32 AND NOT CAF_NO_COMPILER_CHECK)
  try_run(ProgramResult
          CompilationSucceeded
          "${CMAKE_CURRENT_BINARY_DIR}"
          "${CMAKE_CURRENT_SOURCE_DIR}/cmake/get_compiler_version.cpp"
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
endif()
# set optional build flags
set(EXTRA_FLAGS "")
# add "-Werror" flag if --pedantic-build is used
if(CAF_CXX_WARNINGS_AS_ERRORS)
  set(EXTRA_FLAGS "-Werror")
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
               "-Wmissing-braces -Wmissing-field-initializers -Wmultichar "
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
# -fPIC generates warnings on MinGW plus extra setup steps needed on MinGW
if(MINGW)
  add_definitions(-D_WIN32_WINNT=0x0600)
  add_definitions(-DWIN32)
  include(GenerateExportHeader)
  set(LD_FLAGS "ws2_32 -liphlpapi")
  # build static to avoid runtime dependencies to GCC libraries
  set(EXTRA_FLAGS "${EXTRA_FLAGS} -static")
else()
  set(EXTRA_FLAGS "${EXTRA_FLAGS} -fPIC")
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
if(CMAKE_CXX_FLAGS)
  set(CMAKE_CXX_FLAGS_DEBUG          "")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "")
  set(CMAKE_CXX_FLAGS_RELEASE        "")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "")
else()
  set(CMAKE_CXX_FLAGS "-std=c++11 -Wextra -Wall -pedantic ${EXTRA_FLAGS}")
  set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os")
  set(CMAKE_CXX_FLAGS_RELEASE        "-O3 -DNDEBUG")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")
endif()
# set build default build type to RelWithDebInfo if not set
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
# needed by subprojects
set(LD_FLAGS ${LD_FLAGS} ${CMAKE_LD_LIBS})


################################################################################
#                    check for biicode build pipeline                          #
################################################################################
IF(BIICODE)
  # define the biicode target and let it do its thing
  ADD_BII_TARGETS()

  # but first specify the compiled/linker options calculated above
  string(REPLACE " " ";" REPLACED_CXX_FLAGS ${CMAKE_CXX_FLAGS})
  target_compile_options(${BII_BLOCK_TARGET} INTERFACE ${REPLACED_CXX_FLAGS})

  IF (LD_FLAGS STREQUAL "")
    string(REPLACE " " ";" REPLACED_LD_FLAGS ${LD_FLAGS})
    target_link_libraries(${BII_BLOCK_TARGET} INTERFACE ${REPLACED_LD_FLAGS})
  ENDIF()

  RETURN()
ENDIF()

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
#                               add subprojects                                #
################################################################################

# build libcaf_test if not being told otherwise
if(NOT CAF_NO_UNIT_TESTS)
  #message(STATUS "Enter subdirectory libcaf_test")
  #add_subdirectory(libcaf_test)
endif()
# core library
add_subdirectory(libcaf_core)
add_unit_tests("libcaf_core/test/*.cpp")
add_unit_tests("libcaf_io/test/*.cpp")

# set core lib for sub directories
if(NOT CAF_BUILD_STATIC_ONLY)
  set(LIBCAF_CORE_LIBRARY libcaf_core_shared)
else()
  set(LIBCAF_CORE_LIBRARY libcaf_core_static)
endif()
add_subdirectory(libcaf_io)
# set io lib for sub directories
if(NOT CAF_BUILD_STATIC_ONLY)
  set(LIBCAF_IO_LIBRARY libcaf_io_shared)
else()
  set(LIBCAF_IO_LIBRARY libcaf_io_static)
endif()
# set opencl lib for sub directories if not told otherwise
if(NOT CAF_NO_OPENCL
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libcaf_opencl/CMakeLists.txt")
  find_package(OpenCL REQUIRED)
  add_subdirectory(libcaf_opencl)
  if(NOT CAF_BUILD_STATIC_ONLY)
    set(LIBCAF_OPENCL_LIBRARY libcaf_opencl_shared)
  else()
    set(LIBCAF_OPENCL_LIBRARY libcaf_opencl_static)
  endif()
  add_unit_tests("libcaf_opencl/test/*.cpp")
endif()
# tell CMake caf_io depends on caf_core
add_dependencies(libcaf_io libcaf_core)
# set LIBCAF_LIBRARIES for other subprojects
set(LIBCAF_LIBRARIES "${LIBCAF_CORE_LIBRARY}"
                     "${LIBCAF_IO_LIBRARY}"
                     "${LIBCAF_OPENCL_LIBRARY}")
# build examples if not being told otherwise
if(NOT CAF_NO_EXAMPLES)
  add_subdirectory(examples)
  add_dependencies(all_examples libcaf_core libcaf_io)
  if(NOT CAF_NO_OPENCL
     AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libcaf_opencl/CMakeLists.txt")
    add_subdirectory(libcaf_opencl/examples)
  endif()
endif()
# build RIAC if not being told otherwise
if(NOT CAF_NO_RIAC
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libcaf_riac/CMakeLists.txt")
  add_subdirectory(libcaf_riac)
  add_dependencies(libcaf_riac libcaf_core libcaf_io)
  if(NOT CAF_BUILD_STATIC_ONLY)
    set(LIBCAF_LIBRARIES "${LIBCAF_LIBRARIES}" libcaf_riac_shared)
  else()
    set(LIBCAF_LIBRARIES "${LIBCAF_LIBRARIES}" libcaf_riac_static)
  endif()
  # add headers to include directories so other subprojects can use RIAC
  include_directories("${LIBCAF_INCLUDE_DIRS}"
                      "${CMAKE_CURRENT_SOURCE_DIR}/libcaf_riac")
  # add libcaf_riac to the list of caf libraries
  set(CAF_HAS_RIAC yes)
  # add unit tests
  add_unit_tests("libcaf_riac/test/*.cpp")
else()
  set(CAF_HAS_RIAC no)
endif()
# build nexus if not being told otherwise
if(NOT CAF_NO_NEXUS
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/nexus/CMakeLists.txt")
  if(NOT CAF_HAS_RIAC)
    message(WARNING "cannot build nexus without RIAC submodule")
    set(CAF_NO_NEXUS yes)
  else()
    add_subdirectory(nexus)
    add_dependencies(nexus libcaf_riac)
    set(CAF_NO_NEXUS no)
  endif()
else()
  # make sure variable is set for build log
  set(CAF_NO_NEXUS yes)
endif()
# build cash if not being told otherwise
if(NOT CAF_NO_CASH
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cash/CMakeLists.txt")
  if(NOT CAF_HAS_RIAC)
    message(WARNING "cannot build cash without RIAC submodule")
    set(CAF_NO_CASH yes)
  else()
    add_subdirectory(cash)
    add_dependencies(cash libcaf_riac)
    set(CAF_NO_CASH no)
  endif()
else()
  # make sure variable is set for build log
  set(CAF_NO_CASH yes)
endif()
# build benchmarks if not being told otherwise
if(NOT CAF_NO_BENCHMARKS
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/CMakeLists.txt")
  add_subdirectory(benchmarks)
  add_dependencies(all_benchmarks libcaf_io)
else()
  # make sure variable is set for build log
  set(CAF_NO_BENCHMARKS yes)
endif()


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
                        ${LIBCAF_LIBRARIES}
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
               "${suite}" ${ARGN} -- --use-asio)
    endif()
  endmacro ()
  list(LENGTH suites num_suites)
  message(STATUS "Found ${num_suites} test suites")
  foreach(suite ${suites})
    make_test("${suite}")
  endforeach ()
endif()


################################################################################
#                                 LateX setup                                  #
################################################################################

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/manual/variables.tex.in"
               "${CMAKE_CURRENT_SOURCE_DIR}/manual/variables.tex"
               @ONLY)


################################################################################
#                                Doxygen setup                                 #
################################################################################

# check for doxygen and add custom "doc" target to Makefile
find_package(Doxygen)
if(DOXYGEN_FOUND)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in"
                 "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile"
                 @ONLY)
  add_custom_target(doc "${DOXYGEN_EXECUTABLE}"
                    "${CMAKE_HOME_DIRECTORY}/Doxyfile"
                    WORKING_DIRECTORY "${CMAKE_HOME_DIRECTORY}"
                    COMMENT "Generating API documentation with Doxygen"
                    VERBATIM)
endif(DOXYGEN_FOUND)


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
invertYesNo(CAF_NO_EXAMPLES CAF_BUILD_EXAMPLES)
invertYesNo(CAF_NO_UNIT_TESTS CAF_BUILD_UNIT_TESTS)
invertYesNo(CAF_NO_NEXUS CAF_BUILD_NEXUS)
invertYesNo(CAF_NO_CASH CAF_BUILD_CASH)
invertYesNo(CAF_NO_MEM_MANAGEMENT CAF_BUILD_MEM_MANAGEMENT)
invertYesNo(CAF_NO_BENCHMARKS CAF_BUILD_BENCHMARKS)
invertYesNo(CAF_NO_OPENCL CAF_BUILD_OPENCL)
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
        "\nBulid static only: ${CAF_BUILD_STATIC_ONLY}"
        "\nRuntime checks:    ${CAF_ENABLE_RUNTIME_CHECKS}"
        "\nLog level:         ${LOG_LEVEL_STR}"
        "\nWith mem. mgmt.:   ${CAF_BUILD_MEM_MANAGEMENT}"
        "\n"
        "\nBuild examples:    ${CAF_BUILD_EXAMPLES}"
        "\nBuild unit tests:  ${CAF_BUILD_UNIT_TESTS}"
        "\nBuild riac:        ${CAF_HAS_RIAC}"
        "\nBuild nexus:       ${CAF_BUILD_NEXUS}"
        "\nBuild cash:        ${CAF_BUILD_CASH}"
        "\nBuild benchmarks:  ${CAF_BUILD_BENCHMARKS}"
        "\nBuild opencl:      ${CAF_BUILD_OPENCL}"
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
