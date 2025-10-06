set(CAF_ENABLE_EXAMPLES "ON" CACHE BOOL "" FORCE)
set(CAF_ENABLE_ROBOT_TESTS "ON" CACHE BOOL "" FORCE)
set(CAF_ENABLE_RUNTIME_CHECKS "ON" CACHE BOOL "" FORCE)
# TODO: https://github.com/actor-framework/actor-framework/issues/2141
# set(CMAKE_CXX_FLAGS "-Werror -Wno-maybe-uninitialized -Wconversion" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS "-Wconversion" CACHE STRING "" FORCE)
set(CMAKE_BUILD_TYPE "release" CACHE STRING "")
