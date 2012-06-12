cmake_minimum_required(VERSION 2.6)
project(cppa_benchmarks CXX)

# Set up environment paths to cmake modules and libcppa
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(actor_creation actor_creation.cpp)
add_executable(mailbox_performance mailbox_performance.cpp)
add_executable(mixed_case mixed_case.cpp)
add_executable(distributed distributed.cpp)
add_executable(matching matching.cpp)

# search for libs
if(NOT cppa_LIBRARY)
    find_package(Libcppa REQUIRED)
endif (NOT cppa_LIBRARY)

find_package(Boost COMPONENTS thread REQUIRED)

link_directories(${Boost_LIBRARY_DIRS})
include_directories(. ${cppa_INCLUDE} ${Boost_INCLUDE_DIRS})

set(benchmark_LIBS ${CMAKE_DL_LIBS} ${CPPA_LIBRARY} ${Boost_THREAD_LIBRARY})

target_link_libraries(actor_creation ${benchmark_LIBS})
target_link_libraries(mailbox_performance ${benchmark_LIBS})
target_link_libraries(mixed_case ${benchmark_LIBS})
target_link_libraries(distributed ${benchmark_LIBS})
target_link_libraries(matching ${benchmark_LIBS})
