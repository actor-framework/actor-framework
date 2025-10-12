cmake_minimum_required(VERSION 3.16.3)

# 1) Enforce C++20 (needs modern C++ features)
set(CMAKE_CXX_STANDARD        20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS      OFF)

# 2) Set CAF source and build directories
set(CAF_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../../../../actor-framework")
set(CAF_BUILD "${CAF_SRC}/build")


#required since cmake cant seem to find the compiler 
set(CMAKE_CXX_COMPILER "/usr/bin/g++")
set(CMAKE_C_COMPILER "/usr/bin/gcc")

project(CUDA_ACTORS)

find_package(CUDA REQUIRED)
find_package(CUDAToolkit REQUIRED)

include_directories(
  "${CAF_SRC}/include"
  "${CAF_SRC}/libcaf_io"
  "${CAF_SRC}/libcaf_core"
  "${CAF_SRC}/libcaf_opencl"
  "${CAF_BUILD}/libcaf_core"  
  "${CAF_SRC}/libcaf_cuda"
)


# 5) Declare your executable
add_executable(test mmul.example.cpp)
target_compile_definitions(test PRIVATE CAF_ENABLE_LOGGING)

target_link_libraries(test
  PRIVATE
    "${CAF_BUILD}/libcaf_core/libcaf_core.so"
    "${CAF_BUILD}/libcaf_io/libcaf_io.so"
    "${CAF_BUILD}/libcaf_cuda/libcaf_cuda.so"
    CUDA::nvrtc
)

