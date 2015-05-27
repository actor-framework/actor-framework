CAF Unit Testing Framework
==========================

The CAF unit testing framework offers a simple API to write unit tests.
It offers features in a way compliant to Boost.Test library, with similar
concepts and API.

Concepts
--------

- A **check** represents a single verification of boolean operation.
- A **test** contains one or more checks.
- A **suite** groups tests together.
- A **fixture** equips a test with a fixed data environment.

Example
-------

```cpp
#define CAF_SUITE core
#include <caf/test/unit_test.hpp>

CAF_TEST(multiply)
{
  CAF_REQUIRE(0 * 1 == 0);
  CAF_CHECK(42 + 42 == 84);
}

struct fixture {
  fixture() {
    CAF_MESSAGE("entering test");
  }

  ~fixture() {
    CAF_MESSAGE("leaving test");
  }
};

CAF_TEST_FIXTURE_SCOPE(tracing_scope, fixture)

CAF_TEST(divide)
{
  CAF_FAIL(0 / 1 == 0);
  CAF_CHECK(1 / 1 == 0);  // fails
}

CAF_TEST_FIXTURE_SCOPE_END()
```

You can provide your own `main` function by defining `CAF_TEST_NO_MAIN`
before including `unit_test.hpp`. In this case you may refer to the default
test-runner function as `caf::test::main`.
