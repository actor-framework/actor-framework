# - Try to find OpenCL
# Once done this will define
#
#  OpenCL_FOUND        - system has OpenCL
#  OpenCL_INCLUDE_DIRS - the OpenCL include directory
#  OpenCL_LIBRARIES    - link these to use OpenCL
#
# WIN32 should work, but is untested

IF (WIN32)

    FIND_PATH(OpenCL_INCLUDE_DIRS CL/cl.h )

    # TODO this is only a hack assuming the 64 bit library will
    # not be found on 32 bit system
    FIND_LIBRARY(OpenCL_LIBRARIES opencl ) #used to say opencl64
    IF( OpenCL_LIBRARIES )
        FIND_LIBRARY(OpenCL_LIBRARIES opencl ) #used to say opencl32
    ENDIF( OpenCL_LIBRARIES )

ELSE (WIN32)

    FIND_LIBRARY(OpenCL_LIBRARIES OpenCL ENV LD_LIBRARY_PATH)
    message("-- Found OpenCL: ${OpenCL_LIBRARIES}")

ENDIF (WIN32)

SET( OpenCL_FOUND "NO" )
IF(OpenCL_LIBRARIES )
    SET( OpenCL_FOUND "YES" )
ENDIF(OpenCL_LIBRARIES)

MARK_AS_ADVANCED(
  OpenCL_INCLUDE_DIRS
  OpenCL_LIBRARIES
)


