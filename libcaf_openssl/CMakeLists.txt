# -- get header files for creating "proper" XCode projects ---------------------

file(GLOB_RECURSE CAF_OPENSSL_HEADERS "caf/*.hpp")

# -- add library target --------------------------------------------------------

add_library(libcaf_openssl_obj OBJECT ${CAF_OPENSSL_HEADERS}
  src/openssl/manager.cpp
  src/openssl/middleman_actor.cpp
  src/openssl/publish.cpp
  src/openssl/remote_actor.cpp
  src/openssl/session.cpp
)

target_include_directories(libcaf_openssl_obj PRIVATE
                           "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(libcaf_openssl
            "${PROJECT_SOURCE_DIR}/cmake/dummy.cpp"
            $<TARGET_OBJECTS:libcaf_openssl_obj>)

target_include_directories(libcaf_openssl INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

add_library(CAF::openssl ALIAS libcaf_openssl)

if(BUILD_SHARED_LIBS AND NOT WIN32)
  target_compile_options(libcaf_openssl PRIVATE -fPIC)
  target_compile_options(libcaf_openssl_obj PRIVATE -fPIC)
endif()

if(NOT TARGET OpenSSL::SSL OR NOT TARGET OpenSSL::Crypto)
  find_package(OpenSSL REQUIRED)
endif()

target_include_directories(libcaf_openssl_obj PRIVATE
  $<TARGET_PROPERTY:libcaf_core,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:libcaf_io,INTERFACE_INCLUDE_DIRECTORIES>
  ${OPENSSL_INCLUDE_DIR})

target_link_libraries(libcaf_openssl PUBLIC
                      CAF::core CAF::io
                      OpenSSL::SSL OpenSSL::Crypto)

generate_export_header(libcaf_openssl
                       EXPORT_MACRO_NAME CAF_OPENSSL_EXPORT
                       EXPORT_FILE_NAME "${CMAKE_BINARY_DIR}/caf/detail/openssl_export.hpp"
                       STATIC_DEFINE CAF_STATIC_BUILD)

target_compile_definitions(libcaf_openssl_obj PRIVATE libcaf_openssl_EXPORTS)

target_include_directories(libcaf_openssl PUBLIC
                           $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
                           $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
                           $<INSTALL_INTERFACE:include>)

set_target_properties(libcaf_openssl PROPERTIES
                      EXPORT_NAME openssl
                      SOVERSION ${CAF_VERSION}
                      VERSION ${CAF_LIB_VERSION}
                      OUTPUT_NAME caf_openssl)

# -- install library and header files ------------------------------------------

install(FILES "${CMAKE_BINARY_DIR}/caf/detail/openssl_export.hpp"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/caf/detail")

install(TARGETS libcaf_openssl
        EXPORT CAFTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT openssl
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT openssl
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT openssl)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/caf"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        COMPONENT openssl
        FILES_MATCHING PATTERN "*.hpp")

# -- build unit tests ----------------------------------------------------------

if(NOT CAF_ENABLE_TESTING)
  return()
endif()

add_executable(caf-openssl-test
               test/openssl-test.cpp
               ${CAF_OPENSSL_TEST_SOURCES}
               $<TARGET_OBJECTS:libcaf_openssl_obj>)

target_include_directories(caf-openssl-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/test")

target_compile_definitions(caf-openssl-test PRIVATE libcaf_openssl_EXPORTS)

target_link_libraries(caf-openssl-test PRIVATE
                      CAF::io CAF::core CAF::test OpenSSL::SSL OpenSSL::Crypto)

target_include_directories(caf-openssl-test PRIVATE
                           "${CMAKE_CURRENT_SOURCE_DIR}")

caf_add_test_suites(caf-openssl-test
  openssl.authentication
  openssl.remote_actor
)
