cmake_minimum_required(VERSION 2.8)
project(caf_examples_opencl CXX)

if(OpenCL_LIBRARIES AND NOT CAF_NO_EXAMPLES)
  add_custom_target(opencl_examples)
  include_directories(${LIBCAF_INCLUDE_DIRS})
  if(${CMAKE_SYSTEM_NAME} MATCHES "Window")
    set(WSLIB -lws2_32)
  else()
    set(WSLIB)
  endif()
  macro(add name folder)
    add_executable(${name} ${folder}/${name}.cpp ${ARGN})
    target_link_libraries(${name}
                          ${CAF_EXTRA_LDFLAGS}
                          ${CAF_LIBRARIES}
                          ${PTHREAD_LIBRARIES}
                          ${WSLIB}
                          ${OpenCL_LIBRARIES})
    add_dependencies(${name} all_examples)
  endmacro()
  add(proper_matrix .)
  add(simple_matrix .)
  add(scan .)
endif()
