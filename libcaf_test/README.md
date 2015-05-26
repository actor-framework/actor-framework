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

Example
-------

```cpp
#include <caf/test/unit_test.hpp>

CAF_SUITE("core")

CAF_TEST("multiply")
{
  CAF_REQUIRE(0 * 1 == 0);
  CAF_CHECK(42 + 42 == 84);
}

CAF_TEST("divide")
{
  CAF_FAIL(0 / 1 == 0);
  CAF_CHECK(1 / 1 == 0);  // fails
}
```

You can provide your own `main` function by defining `CAF_TEST_NO_MAIN`
before including `unit_test.hpp`. In this case you may refer to the default
test-runner function as `caf::test::main`.
