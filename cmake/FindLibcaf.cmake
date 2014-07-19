# - Try to find user-defined components of libcaf
# Once done this will define
#
#  LIBCAF_INCLUDE_DIRS - include directories for all found components
#  LIBCAF_LIBRARY_$C   - library for component $C
#  LIBCAF_FOUND        - system has libcaf
#  LIBCAF_INCLUDE_DIRS - libcaf include dir
#  LIBCAF_LIBRARIES    - link againgst libcaf
#  LIBCAF_VERSION      - version in {major}.{minor}.{patch} format

foreach (comp ${Libcaf_FIND_COMPONENTS})
  string(TOUPPER "${comp}" UPPERCOMP)
  if ("${comp}" STREQUAL "core")
    set(HDRNAME "caf/all.hpp")
  else ()
    set(HDRNAME "caf/${comp}/all.hpp")
  endif ()
  message(STATUS "Search for libcaf_${comp} and ${HDRNAME}")
  set(HDRHINT "actor-framework/libcaf_${comp}")
  unset(LIBCAF_INCLUDE_DIR)
  find_path(LIBCAF_INCLUDE_DIR_${UPPERCOMP}
            NAMES
              ${HDRNAME}
            HINTS
              ${LIBCAF_ROOT_DIR}/include
              /usr/include
              /usr/local/include
              /opt/local/include
              /sw/include
              ${CMAKE_INSTALL_PREFIX}/include
              ../${HDRHINT}
              ../../${HDRHINT}
              ../../../${HDRHINT})
  mark_as_advanced(LIBCAF_INCLUDE_DIR_${UPPERCOMP})
  if ("${LIBCAF_INCLUDE_DIR_${UPPERCOMP}}" STREQUAL "LIBCAF_INCLUDE_DIR_${UPPERCOMP}-NOTFOUND")
    break ()
  else ()
    message(STATUS "Found headers for ${HDRNAME}")
  endif ()
  find_library(LIBCAF_LIBRARY_${UPPERCOMP}
               NAMES
                 "caf_${comp}"
               HINTS
                 ${LIBCAF_ROOT_DIR}/lib
                 /usr/lib
                 /usr/local/lib
                 /opt/local/lib
                 /sw/lib
                 ${CMAKE_INSTALL_PREFIX}/lib
                 ../actor-framework/build/lib
                 ../../actor-framework/build/lib
                 ../../../actor-framework/build/lib)
  mark_as_advanced(LIBCAF_LIBRARY_${UPPERCOMP})
  if ("${LIBCAF_LIBRARY_${UPPERCOMP}}" STREQUAL "LIBCAF_LIBRARY-NOTFOUND")
    break ()
  else ()
    message(STATUS "Found lib: ${LIBCAF_LIBRARY_${UPPERCOMP}}")
  endif ()
endforeach ()

include(FindPackageHandleStandardArgs)

mark_as_advanced(
  LIBCAF_ROOT_DIR
  LIBCAF_INCLUDE_DIRS)
