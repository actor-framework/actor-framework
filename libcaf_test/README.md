CAF Unit Testing Framework
==========================

The CAF unit testing framework offers a simple API to write unit tests.

Concepts
--------

- A **check** represents a single verification of boolean operation.
- A **test** contains one or more checks.
- A **suite** groups tests together.

Example
-------

```cpp
#include <caf/test/unit_test.h>

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
