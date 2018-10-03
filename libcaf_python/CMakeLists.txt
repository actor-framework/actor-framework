cmake_minimum_required(VERSION 2.8)
project(caf_cash CXX)

# check whether submodules are available
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pybind/CMakeLists.txt")
  message(STATUS "Pybind submodule not loaded, skip libcaf_python.")
  set(CAF_NO_PYTHON yes)
else()
  if(NOT "${CAF_PYTHON_CONFIG_BIN}" STREQUAL "")
    execute_process(COMMAND "${CAF_PYTHON_CONFIG_BIN}" --includes
                    OUTPUT_VARIABLE PYTHON_INCLUDE_FLAGS)
    string(STRIP "${PYTHON_INCLUDE_FLAGS}" PYTHON_INCLUDE_FLAGS)
    execute_process(COMMAND "${CAF_PYTHON_CONFIG_BIN}" --ldflags
                    OUTPUT_VARIABLE PYTHON_LDFLAGS)
    string(STRIP "${PYTHON_LDFLAGS}" PYTHON_LDFLAGS)
    if ("${CAF_EXTRA_LDFLAGS}" STREQUAL "")
      set(CAF_EXTRA_LDFLAGS "${PYTHON_LDFLAGS}")
    else()
      set(CAF_EXTRA_LDFLAGS "${CAF_EXTRA_LDFLAGS} ${PYTHON_LDFLAGS}")
    endif()
    string(REPLACE " -I" ";-I" dir_list ${PYTHON_INCLUDE_FLAGS})
    foreach(flag ${dir_list})
      # strip -I from each path
      string(SUBSTRING "${flag}" 2 -1 dir)
      include_directories("${dir}")
    endforeach()
  else()
    find_package(PythonLibs)
    if (NOT PYTHONLIBS_FOUND)
      message(STATUS "Unable to find Python, disable Python binding")
      message(STATUS "Set CAF_PYTHON_CONFIG_BIN or use './configure --with-python-config=...' to use python-conf")
      set(CAF_NO_PYTHON yes)
    else()
      message(STATUS "Found Python ${PYTHONLIBS_VERSION_STRING}")
      include_directories(${PYTHON_INCLUDE_DIRS})
    endif()
  endif()
endif()


set(CAF_PYTHON_SRCS src/main.cpp)
set(CAF_PYTHON_HDRS)

# add targets to CMake
if(NOT CAF_NO_PYTHON)
  include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
  add_executable(caf-python ${CAF_PYTHON_SRCS} ${CAF_PYTHON_HDRS})
  target_link_libraries(caf-python
                        ${CAF_EXTRA_LDFLAGS}
                        ${CAF_LIBRARY_CORE}
                        ${CAF_LIBRARY_IO}
                        ${CAF_LIBRARY_RIAC}
                        ${PTHREAD_LIBRARIES}
                        ${LIBEDIT_LIBRARIES}
                        ${PYTHON_LIBRARIES})
  install(TARGETS caf-python DESTINATION bin)
else()
  add_custom_target(caf-python SOURCES ${CAF_PYTHON_SRCS} ${CAF_PYTHON_HDRS})
endif()

