cmake_minimum_required(VERSION 2.8)
project(caf_unit_tests_opencl CXX)

if (OPENCL_LIBRARIES)
  add_custom_target(opencl_unit_tests)
  include_directories(${LIBCAF_INCLUDE_DIRS})
  macro(add_unit_test name)
    add_executable(test_${name} test_${name}.cpp ../../unit_testing/test.cpp ${ARGN})
    target_link_libraries(test_${name}
                          ${LD_FLAGS}
                          ${LIBCAF_LIBRARIES}
                          ${PTHREAD_LIBRARIES}
                          ${OPENCL_LIBRARIES})
    add_test(${name} ${EXECUTABLE_OUTPUT_PATH}/test_${name})
    add_dependencies(test_${name} all_unit_tests)
  endmacro()
  add_unit_test(opencl)
endif()
