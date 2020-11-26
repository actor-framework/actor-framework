# -- collect header files ------------------------------------------------------

file(GLOB_RECURSE CAF_OPENSSL_HEADERS "caf/*.hpp")

# -- dependencies --------------------------------------------------------------

if(NOT TARGET OpenSSL::SSL OR NOT TARGET OpenSSL::Crypto)
  find_package(OpenSSL REQUIRED)
endif()

# -- add targets ---------------------------------------------------------------

caf_add_component(
  openssl
  DEPENDENCIES
    PUBLIC
      CAF::io
      OpenSSL::SSL
      OpenSSL::Crypto
    PRIVATE
      CAF::internal
  HEADERS
    ${CAF_OPENSSL_HEADERS}
  SOURCES
    src/openssl/manager.cpp
    src/openssl/middleman_actor.cpp
    src/openssl/publish.cpp
    src/openssl/remote_actor.cpp
    src/openssl/session.cpp
  TEST_SOURCES
    test/openssl-test.cpp
  TEST_SUITES
    openssl.authentication
    openssl.remote_actor)
