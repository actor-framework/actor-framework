# -- get header files for creating "proper" XCode projects ---------------------

file(GLOB_RECURSE CAF_OPENSSL_HEADERS "caf/*.hpp")

# -- dependencies --------------------------------------------------------------

if(NOT TARGET OpenSSL::SSL OR NOT TARGET OpenSSL::Crypto)
  find_package(OpenSSL REQUIRED)
endif()

# -- utility function for setting default properties ---------------------------

function(caf_openssl_set_default_properties)
  foreach(target ${ARGN})
    # Set global defaults and set EXPORTS flag.
    caf_set_default_properties(${target})
    target_compile_definitions(${target} PRIVATE libcaf_openssl_EXPORTS)
    # Pull in public dependencies.
    target_link_libraries(${target} PUBLIC
                          CAF::core CAF::io OpenSSL::SSL OpenSSL::Crypto)
  endforeach()
endfunction()

# -- add library target --------------------------------------------------------

add_library(libcaf_openssl_obj OBJECT ${CAF_OPENSSL_HEADERS}
  src/openssl/manager.cpp
  src/openssl/middleman_actor.cpp
  src/openssl/publish.cpp
  src/openssl/remote_actor.cpp
  src/openssl/session.cpp
)

set_property(TARGET libcaf_openssl_obj PROPERTY POSITION_INDEPENDENT_CODE ON)

add_library(libcaf_openssl
            "${PROJECT_SOURCE_DIR}/cmake/dummy.cpp"
            $<TARGET_OBJECTS:libcaf_openssl_obj>)

caf_openssl_set_default_properties(libcaf_openssl_obj libcaf_openssl)

caf_export_and_install_lib(openssl)

# -- build unit tests ----------------------------------------------------------

if(NOT CAF_ENABLE_TESTING)
  return()
endif()

add_executable(caf-openssl-test
               test/openssl-test.cpp
               ${CAF_OPENSSL_TEST_SOURCES}
               $<TARGET_OBJECTS:libcaf_openssl_obj>)

caf_openssl_set_default_properties(caf-openssl-test)

target_include_directories(caf-openssl-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/test")

target_link_libraries(caf-openssl-test PRIVATE CAF::test)

caf_add_test_suites(caf-openssl-test
  openssl.authentication
  openssl.remote_actor
)
