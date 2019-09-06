project(caf_opencl C CXX)

# get header files; only needed by CMake generators,
# e.g., for creating proper Xcode projects
file(GLOB_RECURSE LIBCAF_OPENCL_HDRS "caf/*.hpp")

# list cpp files excluding platform-dependent files
set(LIBCAF_OPENCL_SRCS
  src/device.cpp
  src/global.cpp
  src/manager.cpp
  src/opencl_err.cpp
  src/platform.cpp
  src/program.cpp
)

message(STATUS "OpenCL_LIBRARIES = ${OpenCL_LIBRARIES}")
add_library(libcaf_opencl ${LIBCAF_OPENCL_SRCS} ${LIBCAF_OPENCL_HDRS})
target_link_libraries(libcaf_opencl PUBLIC caf::core ${OpenCL_LIBRARIES})
target_include_directories(libcaf_opencl PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>
)
set_target_properties(libcaf_opencl
  PROPERTIES
  EXPORT_NAME opencl
  SOVERSION ${CAF_VERSION}
  VERSION ${CAF_VERSION}
  OUTPUT_NAME caf_opencl
)

install(TARGETS libcaf_opencl
  EXPORT CAFTargets
  ARCHIVE DESTINATION lib COMPONENT opencl
  RUNTIME DESTINATION bin COMPONENT opencl
  LIBRARY DESTINATION lib COMPONENT opencl
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/caf"
        DESTINATION include
        COMPONENT opencl
        FILES_MATCHING PATTERN "*.hpp"
)

add_library(caf::opencl ALIAS libcaf_opencl)
if (BUILD_SHARED_LIBS)
  add_library(libcaf_opencl_shared ALIAS libcaf_opencl)
else ()
  add_library(libcaf_opencl_static ALIAS libcaf_opencl)
endif ()
list(APPEND CAF_LIBRARIES libcaf_opencl)
set(CAF_LIBRARIES ${CAF_LIBRARIES} PARENT_SCOPE)

