cmake_minimum_required(VERSION 2.6)
project(cppa_unit_tests CXX)

# set up environment paths to cmake modules and libcppa
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# search for libs
if(NOT cppa_LIBRARY)
    find_package(Libcppa REQUIRED)
endif (NOT cppa_LIBRARY)

find_package(Boost COMPONENTS thread REQUIRED)

link_directories(${Boost_LIBRARY_DIRS})
include_directories(. ${cppa_INCLUDE} ${Boost_INCLUDE_DIRS})

set(EXAMPLE_LIBS ${CMAKE_DL_LIBS} ${CPPA_LIBRARY} ${Boost_THREAD_LIBRARY})

# add binaries
add_executable(test__atom test__atom.cpp)
add_executable(test__intrusive_containers test__intrusive_containers.cpp)
add_executable(test__local_group test__local_group.cpp)
add_executable(test__pattern test__pattern.cpp)
add_executable(test__remote_actor test__remote_actor.cpp ping_pong.cpp)
add_executable(test__serialization test__serialization.cpp)
add_executable(test__uniform_type test__uniform_type.cpp)
add_executable(test__fixed_vector test__fixed_vector.cpp)
add_executable(test__intrusive_ptr test__intrusive_ptr.cpp)
add_executable(test__match test__match.cpp)
add_executable(test__primitive_variant test__primitive_variant.cpp)
add_executable(test__ripemd_160 test__ripemd_160.cpp)
add_executable(test__spawn test__spawn.cpp ping_pong.cpp)
add_executable(test__type_list test__type_list.cpp)
add_executable(test__yield_interface test__yield_interface.cpp)

# link binaries to libraries
target_link_libraries(test__atom ${EXAMPLE_LIBS})
target_link_libraries(test__intrusive_containers ${EXAMPLE_LIBS})
target_link_libraries(test__local_group ${EXAMPLE_LIBS})
target_link_libraries(test__pattern ${EXAMPLE_LIBS})
target_link_libraries(test__remote_actor ${EXAMPLE_LIBS})
target_link_libraries(test__serialization ${EXAMPLE_LIBS})
target_link_libraries(test__uniform_type ${EXAMPLE_LIBS})
target_link_libraries(test__fixed_vector ${EXAMPLE_LIBS})
target_link_libraries(test__intrusive_ptr ${EXAMPLE_LIBS})
target_link_libraries(test__match ${EXAMPLE_LIBS})
target_link_libraries(test__primitive_variant ${EXAMPLE_LIBS})
target_link_libraries(test__ripemd_160 ${EXAMPLE_LIBS})
target_link_libraries(test__spawn ${EXAMPLE_LIBS})
target_link_libraries(test__type_list ${EXAMPLE_LIBS})
target_link_libraries(test__yield_interface ${EXAMPLE_LIBS})

# add binaries to unit tests
add_test(ripemd_160 ${EXECUTABLE_OUTPUT_PATH}/test__ripemd_160)
add_test(atom ${EXECUTABLE_OUTPUT_PATH}/test__atom)
add_test(fixed_vector ${EXECUTABLE_OUTPUT_PATH}/test__fixed_vector)
add_test(intrusive_ptr ${EXECUTABLE_OUTPUT_PATH}/test__intrusive_ptr)
add_test(intrusive_containers ${EXECUTABLE_OUTPUT_PATH}/test__intrusive_containers)
add_test(pattern ${EXECUTABLE_OUTPUT_PATH}/test__pattern)
add_test(match ${EXECUTABLE_OUTPUT_PATH}/test__match)
add_test(primitive_variant ${EXECUTABLE_OUTPUT_PATH}/test__primitive_variant)
add_test(uniform_type ${EXECUTABLE_OUTPUT_PATH}/test__uniform_type)
add_test(type_list ${EXECUTABLE_OUTPUT_PATH}/test__type_list)
add_test(yield_interface ${EXECUTABLE_OUTPUT_PATH}/test__yield_interface)
add_test(local_group ${EXECUTABLE_OUTPUT_PATH}/test__local_group)
add_test(serialization ${EXECUTABLE_OUTPUT_PATH}/test__serialization)
add_test(remote_actor ${EXECUTABLE_OUTPUT_PATH}/test__remote_actor)
add_test(spawn ${EXECUTABLE_OUTPUT_PATH}/test__spawn)
