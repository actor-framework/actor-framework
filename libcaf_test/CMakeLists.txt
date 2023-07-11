# -- collect header files ------------------------------------------------------

file(GLOB_RECURSE CAF_TEST_HEADERS "caf/*.hpp")

# -- add targets ---------------------------------------------------------------

caf_add_component(
  test
  DEPENDENCIES
    PUBLIC
      CAF::core
    PRIVATE
      CAF::internal
  ENUM_TYPES
    test.block_type
  HEADERS
    ${CAF_TEST_HEADERS}
  SOURCES
    src/test/and_given.cpp
    src/test/and_then.cpp
    src/test/and_when.cpp
    src/test/block.cpp
    src/test/context.cpp
    src/test/factory.cpp
    src/test/given.cpp
    src/test/nesting_error.cpp
    src/test/registry.cpp
    src/test/reporter.cpp
    src/test/runnable.cpp
    src/test/runner.cpp
    src/test/scenario.cpp
    src/test/scope.cpp
    src/test/section.cpp
    src/test/test.cpp
    src/test/then.cpp
    src/test/when.cpp
  TEST_SOURCES
    test/test-test.cpp
  TEST_SUITES
    test.block_type
    test.scenario
    test.test)
