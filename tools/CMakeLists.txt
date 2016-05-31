cmake_minimum_required(VERSION 2.8)
project(caf_tools CXX)

add_custom_target(all_tools)

include_directories(${LIBCAF_INCLUDE_DIRS})

if(${CMAKE_SYSTEM_NAME} MATCHES "Window")
  set(WSLIB -lws2_32)
else ()
  set(WSLIB)
endif()

macro(add name)
  add_executable(${name} ${name}.cpp ${ARGN})
  target_link_libraries(${name}
                        ${LD_FLAGS}
                        ${CAF_LIBRARIES}
                        ${PTHREAD_LIBRARIES}
                        ${WSLIB})
  install(FILES ${name}.cpp DESTINATION share/caf/tools/${folder})
  add_dependencies(${name} all_tools)
endmacro()

if(WIN32)
  message(STATUS "skip caf-run (not supported on Windows)")
else()
  add(caf-run)
endif()
