cmake_minimum_required(VERSION 2.8)
project(caf_test C CXX)

file(GLOB LIBCAF_TEST_HDRS "caf/test/*.hpp")
file(GLOB_RECURSE LIBCAF_TEST_SRCS src/unit_test.cpp)

# build shared library if not compiling static only
if (NOT CAF_BUILD_STATIC_ONLY)
  add_library(libcaf_test SHARED ${LIBCAF_TEST_SRCS} ${LIBCAF_TEST_HDRS})
  target_link_libraries(libcaf_test ${LD_FLAGS})
  set(LIBRARY_SOVERSION ${CAF_VERSION_MAJOR})
  set_target_properties(libcaf_test
                        PROPERTIES
                        SOVERSION ${LIBRARY_SOVERSION}
                        VERSION ${CAF_VERSION}
                        OUTPUT_NAME caf_test)
  if(NOT WIN32)
    install(TARGETS libcaf_test LIBRARY DESTINATION lib)
  endif()
endif ()

# build static library only if --build-static or --build-static-only was set
if (CAF_BUILD_STATIC_ONLY OR CAF_BUILD_STATIC)
  add_library(libcaf_testStatic STATIC ${LIBCAF_TEST_HDRS} ${LIBCAF_TEST_SRCS})
  set_target_properties(libcaf_testStatic
                        PROPERTIES OUTPUT_NAME caf_test_static)
  if(NOT WIN32)
    install(TARGETS libcaf_testStatic ARCHIVE DESTINATION lib)
  endif()
endif ()

link_directories(${LD_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# install includes
if(NOT WIN32)
  install(DIRECTORY caf/ DESTINATION include/caf FILES_MATCHING PATTERN "*.hpp")
endif()

# The single unit test binary containing all tests.
file(GLOB_RECURSE tests test.cpp test/*.cpp)
add_executable(caf-test ${tests})
target_link_libraries(caf-test
                      libcaf_test
                      ${LD_FLAGS}
                      ${LIBCAF_CORE_LIBRARY}
                      ${PTHREAD_LIBRARIES})
add_custom_target(all_unit_tests)
add_dependencies(caf-test all_unit_tests)

# Enumerate all test suites.
foreach(test ${tests})
  file(STRINGS ${test} contents)
  foreach(line ${contents})
    if ("${line}" MATCHES "CAF_SUITE(.*)")
      string(REGEX REPLACE ".*\"(.*)\".*" "\\1" suite ${line})
      list(APPEND suites ${suite})
    endif()
  endforeach()
endforeach()
list(REMOVE_DUPLICATES suites)

# Creates one CMake test per test suite.
macro (make_test suite)
  string(REPLACE " " "_" test_name ${suite})
  set(caf_test ${EXECUTABLE_OUTPUT_PATH}/caf-test)
  add_test(${test_name} ${caf_test} -n -v 3 -s "${suite}" ${ARGN})
endmacro ()

foreach(suite ${suites})
  make_test("${suite}")
endforeach ()
