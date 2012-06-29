cmake_minimum_required(VERSION 2.6)
project(cppa_unit_tests CXX)

# Set up environment paths to cmake modules and libcppa
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(unit_tests
               main.cpp
               test__atom.cpp
               test__intrusive_containers.cpp
               test__local_group.cpp
               test__pattern.cpp
               test__remote_actor.cpp
               test__serialization.cpp
               test__tuple.cpp
               test__uniform_type.cpp
               ping_pong.cpp
               test__fixed_vector.cpp
               test__intrusive_ptr.cpp
               test__match.cpp
               test__primitive_variant.cpp
               test__ripemd_160.cpp
               test__spawn.cpp
               test__type_list.cpp
               test__yield_interface.cpp)

# search for libs
if(NOT cppa_LIBRARY)
    find_package(Libcppa REQUIRED)
endif (NOT cppa_LIBRARY)

find_package(Boost COMPONENTS thread REQUIRED)

link_directories(${Boost_LIBRARY_DIRS})
include_directories(. ${cppa_INCLUDE} ${Boost_INCLUDE_DIRS})

set(EXAMPLE_LIBS ${CMAKE_DL_LIBS} ${CPPA_LIBRARY} ${Boost_THREAD_LIBRARY})

target_link_libraries(unit_tests ${EXAMPLE_LIBS})
