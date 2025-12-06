set(CAF_ENABLE_EXAMPLES ON CACHE BOOL "")
set(CAF_ENABLE_ROBOT_TESTS ON CACHE BOOL "")
set(CAF_ENABLE_RUNTIME_CHECKS ON CACHE BOOL "")
# TODO: https://github.com/actor-framework/actor-framework/issues/2140
# set(CMAKE_CXX_FLAGS "-Werror -Wconversion" CACHE STRING "")
set(CMAKE_CXX_FLAGS "-Wconversion" CACHE STRING "")
