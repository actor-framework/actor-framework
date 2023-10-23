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
    caf/test/and_given.cpp
    caf/test/and_then.cpp
    caf/test/and_when.cpp
    caf/test/block.cpp
    caf/test/context.cpp
    caf/test/factory.cpp
    caf/test/fixture/deterministic.cpp
    caf/test/fixture/deterministic.test.cpp
    caf/test/fixture/flow.cpp
    caf/test/fixture/flow.test.cpp
    caf/test/given.cpp
    caf/test/nesting_error.cpp
    caf/test/outline.cpp
    caf/test/outline.test.cpp
    caf/test/registry.cpp
    caf/test/reporter.cpp
    caf/test/requirement_failed.cpp
    caf/test/runnable.cpp
    caf/test/runner.cpp
    caf/test/scenario.cpp
    caf/test/scenario.test.cpp
    caf/test/scope.cpp
    caf/test/section.cpp
    caf/test/test.cpp
    caf/test/test.test.cpp
    caf/test/then.cpp
    caf/test/when.cpp)
