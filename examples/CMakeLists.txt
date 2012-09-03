cmake_minimum_required(VERSION 2.6)
project(cppa_examples CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wextra -Wall -pedantic") 

# Set up environment paths to cmake modules and libcppa
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

link_directories(${Boost_LIBRARY_DIRS})
include_directories(. ${CPPA_INCLUDE} ${Boost_INCLUDE_DIRS} ${PTHREAD_INCLUDE_DIR})

macro(add_example name)
    add_executable(${name} ${name}.cpp ${ARGN})
    target_link_libraries(${name} ${CMAKE_DL_LIBS} ${CPPA_LIBRARY} ${PTHREAD_LIBRARIES})
    add_dependencies(${name} libcppa)
endmacro()

add_example(announce_example_1)
add_example(announce_example_2)
add_example(announce_example_3)
add_example(announce_example_4)
add_example(announce_example_5)
add_example(dancing_kirby)
add_example(dining_philosophers)
add_example(hello_world_example)
add_example(math_actor_example)
add_example(distributed_math_actor_example)
add_example(group_server)
add_example(group_chat)
