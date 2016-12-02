# - Try to find hwloc
# Once done this will define
#
#  Hwloc_FOUND        - system has OpenCL
#  Hwloc_INCLUDE_DIRS - the OpenCL include directory
#  Hwloc_LIBRARIES    - link these to use OpenCL
#  Hwloc_VERSION      - version
#
# WIN32 should work, but is untested

if (WIN32)
  find_path(Hwloc_INCLUDE_DIRS 
    NAMES   
      hwloc.h
    PATHS
      ENV "PROGRAMFILES(X86)"
      ENV HWLOC_ROOT
    PATH_SUFFIXES
      include
  ) 
  find_library(Hwloc_LIBRARY
    NAMES 
      libhwloc.lib
    PATHS
      ENV "PROGRAMFILES(X86)"
      ENV HWLOC_ROOT
    PATH_SUFFIXES
      lib
  )
  find_program(HWLOC_INFO_EXECUTABLE
    NAMES 
      hwloc-info
    PATHS
      ENV HWLOC_ROOT 
    PATH_SUFFIXES
      bin
  )
  if(HWLOC_INFO_EXECUTABLE)
    execute_process(
      COMMAND ${HWLOC_INFO_EXECUTABLE} "--version" 
      OUTPUT_VARIABLE HWLOC_VERSION_LINE 
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    string(REGEX MATCH "([0-9]+.[0-9]+)$" 
      Hwloc_VERSION "${HWLOC_VERSION_LINE}")
    unset(HWLOC_VERSION_LINE)
  endif()
  set(Hwloc_LIBRARIES ${Hwloc_LIBRARY})
  set(Hwloc_INCLUDE_DIRS ${Hwloc_INCLUDE_DIR})
  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(
    Hwloc
    FOUND_VAR Hwloc_FOUND
    REQUIRED_VARS Hwloc_LIBRARY Hwloc_INCLUDE_DIR
    VERSION_VAR Hwloc_VERSION
  )
  mark_as_advanced(
    Hwloc_INCLUDE_DIR
    Hwloc_LIBRARY
  )
else()
  find_package(PkgConfig)
  pkg_check_modules(Hwloc QUIET hwloc)
  if(Hwloc_FOUND)
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(Hwloc DEFAULT_MSG Hwloc_LIBRARIES)
    message(STATUS "Found hwloc version ${Hwloc_VERSION}")
  endif() 
endif()
