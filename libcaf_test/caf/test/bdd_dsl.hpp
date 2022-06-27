#pragma once

#include "caf/detail/pp.hpp"
#include "caf/test/dsl.hpp"

#define SCENARIO(description)                                                  \
  namespace {                                                                  \
  struct CAF_UNIQUE(test) : caf_test_case_auto_fixture {                       \
    void run_test_impl();                                                      \
  };                                                                           \
  ::caf::test::detail::adder<::caf::test::test_impl<CAF_UNIQUE(test)>>         \
    CAF_UNIQUE(a){CAF_XSTR(CAF_SUITE), "SCENARIO " description, false};        \
  }                                                                            \
  void CAF_UNIQUE(test)::run_test_impl()

#define GIVEN(description)                                                     \
  CAF_MESSAGE("GIVEN " description);                                           \
  if (true)

#define WHEN(description)                                                      \
  CAF_MESSAGE("WHEN " description);                                            \
  if (true)

#define AND_WHEN(description)                                                  \
  CAF_MESSAGE("AND WHEN " description);                                        \
  if (true)

#define THEN(description)                                                      \
  CAF_MESSAGE("THEN " description);                                            \
  if (true)

#define AND_THEN(description)                                                  \
  CAF_MESSAGE("AND THEN " description);                                        \
  if (true)

#define AND(description)                                                       \
  {}                                                                           \
  CAF_MESSAGE("AND " description);                                             \
  if (true)

#define CHECK(what) CAF_CHECK(what)
#define CHECK_EQ(lhs, rhs) CAF_CHECK_EQUAL(lhs, rhs)
#define CHECK_NE(lhs, rhs) CAF_CHECK_NOT_EQUAL(lhs, rhs)
#define CHECK_LT(lhs, rhs) CAF_CHECK_LESS(lhs, rhs)
#define CHECK_LE(lhs, rhs) CAF_CHECK_LESS_OR_EQUAL(lhs, rhs)
#define CHECK_GT(lhs, rhs) CAF_CHECK_GREATER(lhs, rhs)
#define CHECK_GE(lhs, rhs) CAF_CHECK_GREATER_OR_EQUAL(lhs, rhs)

#ifdef CAF_ENABLE_EXCEPTIONS

#  define CHECK_NOTHROW(expr) CAF_CHECK_NOTHROW(expr)
#  define CHECK_THROWS_AS(expr, type) CAF_CHECK_THROWS_AS(expr, type)
#  define CHECK_THROWS_WITH(expr, msg) CAF_CHECK_THROWS_WITH(expr, msg)
#  define CHECK_THROWS_WITH_AS(expr, msg, type)                                \
    CAF_CHECK_THROWS_WITH_AS(expr, msg, type)

#endif // CAF_ENABLE_EXCEPTIONS

#define REQUIRE(what) CAF_REQUIRE(what)
#define REQUIRE_EQ(lhs, rhs) CAF_REQUIRE_EQUAL(lhs, rhs)
#define REQUIRE_NE(lhs, rhs) CAF_REQUIRE_NOT_EQUAL(lhs, rhs)
#define REQUIRE_LT(lhs, rhs) CAF_REQUIRE_LESS(lhs, rhs)
#define REQUIRE_LE(lhs, rhs) CAF_REQUIRE_LESS_OR_EQUAL(lhs, rhs)
#define REQUIRE_GT(lhs, rhs) CAF_REQUIRE_GREATER(lhs, rhs)
#define REQUIRE_GE(lhs, rhs) CAF_REQUIRE_GREATER_OR_EQUAL(lhs, rhs)

#define MESSAGE(what) CAF_MESSAGE(what)

#define FAIL(what) CAF_FAIL(what)

#define BEGIN_FIXTURE_SCOPE(fixture_class)                                     \
  CAF_TEST_FIXTURE_SCOPE(CAF_PP_UNIFYN(tests), fixture_class)

#define END_FIXTURE_SCOPE() CAF_TEST_FIXTURE_SCOPE_END()
