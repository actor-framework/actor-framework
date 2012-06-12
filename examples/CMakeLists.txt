cmake_minimum_required(VERSION 2.6)
project(cppa_examples CXX)

# Set up environment paths to cmake modules and libcppa
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(announce_example_1 announce_example_1.cpp)
add_executable(announce_example_2 announce_example_2.cpp)
add_executable(announce_example_3 announce_example_3.cpp)
add_executable(announce_example_4 announce_example_4.cpp)
add_executable(announce_example_5 announce_example_5.cpp)
add_executable(dancing_kirby dancing_kirby.cpp)
add_executable(dining_philosophers dining_philosophers.cpp)
add_executable(hello_world_example hello_world_example.cpp)
add_executable(math_actor_example math_actor_example.cpp)

# search for libs
if(NOT cppa_LIBRARY)
    find_package(Libcppa REQUIRED)
endif (NOT cppa_LIBRARY)

find_package(Boost COMPONENTS thread REQUIRED)

link_directories(${Boost_LIBRARY_DIRS})
include_directories(. ${cppa_INCLUDE} ${Boost_INCLUDE_DIRS})

set(EXAMPLE_LIBS ${CMAKE_DL_LIBS} ${CPPA_LIBRARY} ${Boost_THREAD_LIBRARY})

target_link_libraries(announce_example_1 ${EXAMPLE_LIBS})
target_link_libraries(announce_example_2 ${EXAMPLE_LIBS})
target_link_libraries(announce_example_3 ${EXAMPLE_LIBS})
target_link_libraries(announce_example_4 ${EXAMPLE_LIBS})
target_link_libraries(announce_example_5 ${EXAMPLE_LIBS})
target_link_libraries(dancing_kirby ${EXAMPLE_LIBS})
target_link_libraries(dining_philosophers ${EXAMPLE_LIBS})
target_link_libraries(hello_world_example ${EXAMPLE_LIBS})
target_link_libraries(math_actor_example ${EXAMPLE_LIBS})
