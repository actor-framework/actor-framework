# -- collect header files ------------------------------------------------------

file(GLOB_RECURSE CAF_OPENSSL_HEADERS "caf/*.hpp")

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
    caf/openssl/manager.cpp
    caf/openssl/middleman_actor.cpp
    caf/openssl/publish.cpp
    caf/openssl/remote_actor.cpp
    caf/openssl/session.cpp
  LEGACY_TEST_SUITES
    tests/legacy/openssl-test.cpp
  LEGACY_TEST_SUITES
    openssl.authentication
    openssl.remote_actor)
