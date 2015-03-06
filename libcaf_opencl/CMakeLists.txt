cmake_minimum_required(VERSION 2.8)
project(caf_opencl C CXX)

# get header files; only needed by CMake generators,
# e.g., for creating proper Xcode projects
file(GLOB LIBCAF_OPENCL_HDRS "caf/opencl/*.hpp")

if (OpenCL_LIBRARIES)
  # list cpp files excluding platform-dependent files
  set (LIBCAF_OPENCL_SRCS
       src/global.cpp
       src/opencl_metainfo.cpp
       src/program.cpp
       src/opencl_err.cpp
       )
  # build shared library if not compiling static only
  if(NOT "${CAF_BUILD_STATIC_ONLY}" STREQUAL "yes")
    add_library(libcaf_opencl SHARED ${LIBCAF_OPENCL_SRCS}
                                     ${LIBCAF_OPENCL_HDRS}
                                     ${OpenCL_INCLUDE_DIRS})
    target_link_libraries(libcaf_opencl ${LD_FLAGS}
                                        ${LIBCAF_CORE_LIBRARY}
                                        ${OpenCL_LIBRARIES})
    set(LIBRARY_SOVERSION "${CAF_VERSION_MAJOR}")
    set_target_properties(libcaf_opencl
                          PROPERTIES
                          SOVERSION "${LIBRARY_SOVERSION}"
                          VERSION "${CAF_VERSION}"
                          OUTPUT_NAME caf_opencl)
    if(NOT MINGW)
      install(TARGETS libcaf_opencl LIBRARY DESTINATION lib)
    endif()
  endif()
  # build static library only if --build-static or --build-static-only was set
  if(CAF_BUILD_STATIC_ONLY OR CAF_BUILD_STATIC)
    add_library(libcaf_openclStatic STATIC ${LIBCAF_OPENCL_HDRS} ${LIBCAF_OPENCL_SRCS})
    set_target_properties(libcaf_openclStatic PROPERTIES OUTPUT_NAME caf_opencl_static)
    install(TARGETS libcaf_openclStatic ARCHIVE DESTINATION lib)
  endif()
  link_directories(${LD_DIRS})
  include_directories(. ${INCLUDE_DIRS})
endif()

# install includes
install(DIRECTORY caf/ DESTINATION include/caf FILES_MATCHING PATTERN "*.hpp")
install(DIRECTORY cppa/ DESTINATION include/cppa FILES_MATCHING PATTERN "*.hpp")
