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

# build shared library if not compiling static only
if(NOT CAF_BUILD_STATIC_ONLY)
  add_library(libcaf_opencl_shared SHARED ${LIBCAF_OPENCL_SRCS}
              ${LIBCAF_OPENCL_HDRS} ${OpenCL_INCLUDE_DIRS})
  target_link_libraries(libcaf_opencl_shared ${CAF_EXTRA_LDFLAGS}
                                             ${CAF_LIBRARY_CORE}
                                             ${OpenCL_LIBRARIES})
  target_link_libraries(libcaf_opencl_shared
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
  )
  set_target_properties(libcaf_opencl_shared
                        PROPERTIES
                        SOVERSION "${CAF_VERSION}"
                        VERSION "${CAF_VERSION}"
                        OUTPUT_NAME caf_opencl)
  install(TARGETS libcaf_opencl_shared LIBRARY DESTINATION lib)
endif()

# build static library only if --build-static or --build-static-only was set
if(CAF_BUILD_STATIC_ONLY OR CAF_BUILD_STATIC)
  add_library(libcaf_opencl_static STATIC ${LIBCAF_OPENCL_SRCS}
                                          ${LIBCAF_OPENCL_HDRS}
                                          ${OpenCL_INCLUDE_DIRS})
  target_link_libraries(libcaf_opencl_static ${CAF_EXTRA_LDFLAGS}
                                             ${CAF_LIBRARY_CORE_STATIC}
                                             ${OpenCL_LIBRARIES})
  target_link_libraries(libcaf_opencl_static
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
  )
  set_target_properties(libcaf_opencl_static PROPERTIES OUTPUT_NAME caf_opencl_static)
  install(TARGETS libcaf_opencl_static ARCHIVE DESTINATION lib)
endif()

# install includes
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/caf
        DESTINATION include
        FILES_MATCHING PATTERN "*.hpp")
