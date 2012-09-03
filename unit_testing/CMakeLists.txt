cmake_minimum_required(VERSION 2.6)
project(cppa_unit_tests CXX)


# set up environment paths to cmake modules and libcppa
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# search for libs
if(NOT cppa_LIBRARY)
    find_package(Libcppa REQUIRED)
endif (NOT cppa_LIBRARY)

#find_package(Boost COMPONENTS thread REQUIRED)
find_package(Boost REQUIRED)

find_package(PTHREAD REQUIRED)

link_directories(${Boost_LIBRARY_DIRS})
include_directories(. ${cppa_INCLUDE} ${Boost_INCLUDE_DIRS} ${PTHREAD_INCLUDE_DIR})

set(EXAMPLE_LIBS ${CMAKE_DL_LIBS} ${CPPA_LIBRARY} ${PTHREAD_LIBRARIES})

macro(add_unit_test name)
    add_executable(test__${name} test__${name}.cpp ${ARGN})
    target_link_libraries(test__${name} ${EXAMPLE_LIBS} ${CMAKE_THREAD_LIBS_INIT})
    add_test(${name} ${EXECUTABLE_OUTPUT_PATH}/test__${name})
    add_dependencies(test__${name} libcppa)
endmacro()

add_unit_test(ripemd_160)
add_unit_test(atom)
add_unit_test(type_list)
add_unit_test(intrusive_containers)
add_unit_test(pattern)
add_unit_test(serialization)
add_unit_test(uniform_type)
add_unit_test(fixed_vector)
add_unit_test(intrusive_ptr)
add_unit_test(match)
add_unit_test(primitive_variant)
add_unit_test(yield_interface)
add_unit_test(spawn ping_pong.cpp)
add_unit_test(local_group)
add_unit_test(sync_send)
add_unit_test(remote_actor ping_pong.cpp)
