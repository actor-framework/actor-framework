cmake_minimum_required(VERSION 2.8)
project(caf_opencl C CXX)

# get header files; only needed by CMake generators,
# e.g., for creating proper Xcode projects
file(GLOB_RECURSE LIBCAF_OPENCL_HDRS "caf/*.hpp")

add_custom_target(libcaf_opencl)

# list cpp files excluding platform-dependent files
set (LIBCAF_OPENCL_SRCS
     src/global.cpp
     src/manager.cpp
     src/program.cpp
     src/opencl_err.cpp
     src/platform.cpp
     src/device.cpp)
# build shared library if not compiling static only
if(NOT CAF_BUILD_STATIC_ONLY)
  add_library(libcaf_opencl_shared SHARED ${LIBCAF_OPENCL_SRCS}
              ${LIBCAF_OPENCL_HDRS} ${OpenCL_INCLUDE_DIRS})
  target_link_libraries(libcaf_opencl_shared ${LD_FLAGS}
                                             ${CAF_LIBRARY_CORE}
                                             ${OpenCL_LIBRARIES})
  set_target_properties(libcaf_opencl_shared
                        PROPERTIES
                        SOVERSION "${CAF_VERSION}"
                        VERSION "${CAF_VERSION}"
                        OUTPUT_NAME caf_opencl)
  if(NOT WIN32)
    install(TARGETS libcaf_opencl_shared LIBRARY DESTINATION lib)
  endif()
endif()
# build static library only if --build-static or --build-static-only was set
if(CAF_BUILD_STATIC_ONLY OR CAF_BUILD_STATIC)
  add_library(libcaf_opencl_static STATIC ${LIBCAF_OPENCL_HDRS} ${LIBCAF_OPENCL_SRCS})
  target_link_libraries(libcaf_opencl_static ${LD_FLAGS}
                                             ${CAF_LIBRARY_CORE_STATIC}
                                             ${OpenCL_LIBRARIES})
  set_target_properties(libcaf_opencl_static PROPERTIES OUTPUT_NAME caf_opencl_static)
  install(TARGETS libcaf_opencl_static ARCHIVE DESTINATION lib)
endif()
link_directories(${LD_DIRS})
include_directories(. ${INCLUDE_DIRS})
# install includes
install(DIRECTORY caf/ DESTINATION include/caf FILES_MATCHING PATTERN "*.hpp")
