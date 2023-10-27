// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/flow/observable.hpp"

#include "caf/test/fixture/flow.hpp"
#include "caf/test/nil.hpp"
#include "caf/test/test.hpp"

using caf::test::nil;
using std::vector;

using namespace caf;

namespace {

WITH_FIXTURE(test::fixture::flow) {

// Note: we always double the checks for the operator-under-test by calling it
//       once on a blueprint and once on an actual observable. This ensures that
//       the blueprint and the observable both offer the same functionality.

TEST("element_at(n) only takes the element with index n") {
  SECTION("element_at(0) picks the first element") {
    SECTION("blueprint") {
      check_eq(collect(range(1, 1).element_at(0)), vector{1});
      check_eq(collect(range(1, 2).element_at(0)), vector{1});
      check_eq(collect(range(1, 3).element_at(0)), vector{1});
    }
    SECTION("observable") {
      check_eq(collect(build(range(1, 1)).element_at(0)), vector{1});
      check_eq(collect(build(range(1, 2)).element_at(0)), vector{1});
      check_eq(collect(build(range(1, 3)).element_at(0)), vector{1});
    }
  }
  SECTION("element_at(1) picks the second element") {
    SECTION("blueprint") {
      check_eq(collect(range(1, 2).element_at(1)), vector{2});
      check_eq(collect(range(1, 3).element_at(1)), vector{2});
    }
    SECTION("observable") {
      check_eq(collect(build(range(1, 2)).element_at(1)), vector{2});
      check_eq(collect(build(range(1, 3)).element_at(1)), vector{2});
    }
  }
  SECTION("element_at(n) does not pick any element if n >= m") {
    SECTION("blueprint") {
      check_eq(collect(range(1, 1).element_at(1)), nil);
      check_eq(collect(range(1, 2).element_at(2)), nil);
      check_eq(collect(range(1, 3).element_at(3)), nil);
    }
    SECTION("observable") {
      check_eq(collect(build(range(1, 1)).element_at(1)), nil);
      check_eq(collect(build(range(1, 2)).element_at(2)), nil);
      check_eq(collect(build(range(1, 3)).element_at(3)), nil);
    }
  }
  SECTION("element_at(n) propagates errors") {
    SECTION("blueprint") {
      check_eq(collect(obs_error().element_at(1)), sec::runtime_error);
    }
    SECTION("observable") {
      check_eq(collect(build(obs_error()).element_at(1)), sec::runtime_error);
    }
  }
}

TEST("reduce(init, fn) combines all items into one final value using fn") {
  std::plus<int> adder;
  SECTION("reduce(init, fn) emits final value for a range of size m") {
    SECTION("blueprint") {
      check_eq(collect(range(1, 1).reduce(0, adder)), vector{1});
      check_eq(collect(range(1, 2).reduce(0, adder)), vector{3});
      check_eq(collect(range(1, 3).reduce(0, adder)), vector{6});
    }
    SECTION("observable") {
      check_eq(collect(build(range(1, 1)).reduce(0, adder)), vector{1});
      check_eq(collect(build(range(1, 2)).reduce(0, adder)), vector{3});
      check_eq(collect(build(range(1, 3)).reduce(0, adder)), vector{6});
    }
  }
  SECTION("reduce(init, fn) emits 0 for a range of size 0") {
    SECTION("blueprint") {
      check_eq(collect(range(1, 0).reduce(0, adder)), vector{0});
    }
    SECTION("observable") {
      check_eq(collect(build(range(1, 0)).reduce(0, adder)), vector{0});
    }
  }
  SECTION("reduce(init, fn) propagates errors") {
    SECTION("blueprint") {
      check_eq(collect(range(1, 1).concat(obs_error()).reduce(0, adder)),
               sec::runtime_error);
      check_eq(collect(range(1, 3).concat(obs_error()).reduce(0, adder)),
               sec::runtime_error);
      check_eq(collect(obs_error().reduce(0, adder)), sec::runtime_error);
    }
    SECTION("observable") {
      check_eq(collect(build(range(1, 1).concat(obs_error())).reduce(0, adder)),
               sec::runtime_error);
      check_eq(collect(build(range(1, 3).concat(obs_error())).reduce(0, adder)),
               sec::runtime_error);
      check_eq(collect(build(obs_error()).reduce(0, adder)),
               sec::runtime_error);
    }
  }
}

TEST("scan(init, fn) creates successive reduced values using fn") {
  std::plus<int> adder;
  SECTION("scan(init, fn) emits successive value for a range of size m") {
    SECTION("blueprint") {
      check_eq(collect(range(1, 1).scan(0, adder)), vector{1});
      check_eq(collect(range(1, 2).scan(0, adder)), vector{1, 3});
      check_eq(collect(range(1, 3).scan(0, adder)), vector{1, 3, 6});
    }
    SECTION("observable") {
      check_eq(collect(build(range(1, 1)).scan(0, adder)), vector{1});
      check_eq(collect(build(range(1, 2)).scan(0, adder)), vector{1, 3});
      check_eq(collect(build(range(1, 3)).scan(0, adder)), vector{1, 3, 6});
    }
  }
  SECTION("scan(init, fn) emits 0 for a range of size 0") {
    SECTION("blueprint") {
      check_eq(collect(range(1, 0).scan(0, adder)), nil);
    }
    SECTION("observable") {
      check_eq(collect(build(range(1, 0)).scan(0, adder)), nil);
    }
  }
  SECTION("scan(init, fn) propagates errors") {
    SECTION("blueprint") {
      check_eq(collect(range(1, 1).concat(obs_error()).scan(0, adder)),
               sec::runtime_error);
      check_eq(collect(range(1, 3).concat(obs_error()).scan(0, adder)),
               sec::runtime_error);
      check_eq(collect(obs_error().scan(0, adder)), sec::runtime_error);
    }
    SECTION("observable") {
      check_eq(collect(build(range(1, 1).concat(obs_error())).scan(0, adder)),
               sec::runtime_error);
      check_eq(collect(build(range(1, 3).concat(obs_error())).scan(0, adder)),
               sec::runtime_error);
      check_eq(collect(build(obs_error()).scan(0, adder)), sec::runtime_error);
    }
  }
}

TEST("skip(n) skips the first n elements in a range of size m") {
  SECTION("skip(0) does nothing") {
    SECTION("blueprint") {
      check_eq(collect(range(1, 0).skip(0)), nil);
      check_eq(collect(range(1, 1).skip(0)), vector{1});
      check_eq(collect(range(1, 2).skip(0)), vector{1, 2});
      check_eq(collect(range(1, 3).skip(0)), vector{1, 2, 3});
    }
    SECTION("observable") {
      check_eq(collect(build(range(1, 0)).skip(0)), nil);
      check_eq(collect(build(range(1, 1)).skip(0)), vector{1});
      check_eq(collect(build(range(1, 2)).skip(0)), vector{1, 2});
      check_eq(collect(build(range(1, 3)).skip(0)), vector{1, 2, 3});
    }
  }
  SECTION("skip(n) truncates the first n elements if n < m") {
    SECTION("blueprint") {
      check_eq(collect(range(1, 3).skip(1)), vector{2, 3});
      check_eq(collect(range(1, 3).skip(2)), vector{3});
    }
    SECTION("observable") {
      check_eq(collect(build(range(1, 3)).skip(1)), vector{2, 3});
      check_eq(collect(build(range(1, 3)).skip(2)), vector{3});
    }
  }
  SECTION("skip(n) drops all elements if n >= m") {
    SECTION("blueprint") {
      check_eq(collect(range(1, 3).skip(3)), nil);
      check_eq(collect(range(1, 3).skip(4)), nil);
    }
    SECTION("observable") {
      check_eq(collect(build(range(1, 3)).skip(3)), nil);
      check_eq(collect(build(range(1, 3)).skip(4)), nil);
    }
  }
  SECTION("skip(n) stops early when the next operator stops early") {
    SECTION("blueprint") {
      check_eq(collect(range(1, 5).skip(2).take(0)), nil);
      check_eq(collect(range(1, 5).skip(2).take(1)), vector{3});
      check_eq(collect(range(1, 5).skip(2).take(2)), vector{3, 4});
      check_eq(collect(range(1, 5).skip(2).take(3)), vector{3, 4, 5});
      check_eq(collect(range(1, 5).skip(2).take(4)), vector{3, 4, 5});
    }
    SECTION("observable") {
      check_eq(collect(build(range(1, 5)).skip(2).take(0)), nil);
      check_eq(collect(build(range(1, 5)).skip(2).take(1)), vector{3});
      check_eq(collect(build(range(1, 5)).skip(2).take(2)), vector{3, 4});
      check_eq(collect(build(range(1, 5)).skip(2).take(3)), vector{3, 4, 5});
      check_eq(collect(build(range(1, 5)).skip(2).take(4)), vector{3, 4, 5});
    }
  }
  SECTION("skip(n) forwards errors") {
    SECTION("blueprint") {
      check_eq(collect(obs_error().skip(0)), sec::runtime_error);
      check_eq(collect(obs_error().skip(1)), sec::runtime_error);
    }
    SECTION("observable") {
      check_eq(collect(build(obs_error()).skip(0)), sec::runtime_error);
      check_eq(collect(build(obs_error()).skip(1)), sec::runtime_error);
    }
  }
}

TEST("first() returns the first element in a range of size m") {
  SECTION("first() returns the first element in a range of size >= 1") {
    SECTION("blueprint") {
      check_eq(collect(range(1, 1).first()), vector{1});
      check_eq(collect(range(1, 2).first()), vector{1});
      check_eq(collect(range(1, 3).first()), vector{1});
    }
    SECTION("observable") {
      check_eq(collect(build(range(1, 1)).first()), vector{1});
      check_eq(collect(build(range(1, 2)).first()), vector{1});
      check_eq(collect(build(range(1, 3)).first()), vector{1});
    }
  }
  SECTION("first() returns an empty range if the input is empty") {
    SECTION("blueprint") {
      check_eq(collect(range(1, 0).first()), nil);
    }
    SECTION("observable") {
      check_eq(collect(build(range(1, 0)).first()), nil);
    }
  }
}

TEST("last() returns the last element in a range of size m") {
  SECTION("last() returns the last element in a range of size >= 1") {
    SECTION("blueprint") {
      check_eq(collect(range(1, 1).last()), vector{1});
      check_eq(collect(range(1, 2).last()), vector{2});
      check_eq(collect(range(1, 3).last()), vector{3});
    }
    SECTION("observable") {
      check_eq(collect(build(range(1, 1)).last()), vector{1});
      check_eq(collect(build(range(1, 2)).last()), vector{2});
      check_eq(collect(build(range(1, 3)).last()), vector{3});
    }
  }
  SECTION("last() returns an empty range if the input is empty") {
    SECTION("blueprint") {
      check_eq(collect(range(1, 0).last()), nil);
    }
    SECTION("observable") {
      check_eq(collect(build(range(1, 0)).last()), nil);
    }
  }
}

TEST("on_error_complete() suppresses errors") {
  SECTION("on_error_complete() does nothing if no error occurs") {
    SECTION("blueprint") {
      check_eq(collect(range(1, 1).on_error_complete()), vector{1});
      check_eq(collect(range(1, 2).on_error_complete()), vector{1, 2});
      check_eq(collect(range(1, 3).on_error_complete()), vector{1, 2, 3});
      check_eq(collect(range(1, 0).on_error_complete()), nil);
    }
    SECTION("observable") {
      check_eq(collect(build(range(1, 1)).on_error_complete()), vector{1});
      check_eq(collect(build(range(1, 2)).on_error_complete()), vector{1, 2});
      check_eq(collect(build(range(1, 3)).on_error_complete()),
               vector{1, 2, 3});
      check_eq(collect(build(range(1, 0)).on_error_complete()), nil);
    }
  }
  SECTION("on_error_complete() does not propagate errors") {
    SECTION("blueprint") {
      check_eq(collect(range(1, 1).concat(obs_error()).on_error_complete()),
               vector{1});
      check_eq(collect(range(1, 2).concat(obs_error()).on_error_complete()),
               vector{1, 2});
      check_eq(collect(range(1, 3).concat(obs_error()).on_error_complete()),
               vector{1, 2, 3});
      check_eq(collect(obs_error().on_error_complete()), nil);
    }
    SECTION("observable") {
      check_eq(collect(
                 build(range(1, 1).concat(obs_error())).on_error_complete()),
               vector{1});
      check_eq(collect(
                 build(range(1, 2).concat(obs_error())).on_error_complete()),
               vector{1, 2});
      check_eq(collect(
                 build(range(1, 3).concat(obs_error())).on_error_complete()),
               vector{1, 2, 3});
      check_eq(collect(build(obs_error()).on_error_complete()), nil);
    }
  }
  SECTION("on_error_complete() completes on error") {
    SECTION("blueprint") {
      check_eq(collect(range(1, 2)
                         .concat(obs_error())
                         .concat(range(1, 2))
                         .on_error_complete()),
               vector{1, 2});
      check_eq(collect(range(1, 3)
                         .concat(obs_error())
                         .concat(range(1, 3))
                         .on_error_complete()),
               vector{1, 2, 3});
    }
    SECTION("observable") {
      check_eq(collect(
                 build(range(1, 2).concat(obs_error()).concat(range(1, 2)))
                   .on_error_complete()),
               vector{1, 2});
      check_eq(collect(
                 build(range(1, 3).concat(obs_error()).concat(range(1, 3)))
                   .on_error_complete()),
               vector{1, 2, 3});
    }
  }
}

TEST("on_error_return_item() replaces an error with a value") {
  SECTION("on_error_return_item() does nothing if no error occurs") {
    SECTION("blueprint") {
      check_eq(collect(range(1, 1).on_error_return_item(42)), vector{1});
      check_eq(collect(range(1, 2).on_error_return_item(42)), vector{1, 2});
      check_eq(collect(range(1, 3).on_error_return_item(42)), vector{1, 2, 3});
      check_eq(collect(range(1, 0).on_error_return_item(42)), nil);
    }
    SECTION("observable") {
      check_eq(collect(build(range(1, 1)).on_error_return_item(42)), vector{1});
      check_eq(collect(build(range(1, 2)).on_error_return_item(42)),
               vector{1, 2});
      check_eq(collect(build(range(1, 3)).on_error_return_item(42)),
               vector{1, 2, 3});
      check_eq(collect(build(range(1, 0)).on_error_return_item(42)), nil);
    }
  }
  SECTION("on_error_return_item() returns the item when an error occurs") {
    SECTION("blueprint") {
      check_eq(collect(obs_error().on_error_return_item(42)), vector{42});
      check_eq(collect(range(1, 2)
                         .concat(obs_error())
                         .concat(range(1, 2))
                         .on_error_return_item(42)),
               vector{1, 2, 42});
      check_eq(collect(range(1, 3)
                         .concat(obs_error())
                         .concat(range(1, 3))
                         .on_error_return_item(42)),
               vector{1, 2, 3, 42});
      check_eq(
        collect(
          range(1, 2).concat(obs_error()).on_error_return_item(42).take(2)),
        vector{1, 2});
      check_eq(
        collect(
          range(1, 3).concat(obs_error()).on_error_return_item(42).take(3)),
        vector{1, 2, 3});
    }
    SECTION("observable") {
      check_eq(collect(build(obs_error()).on_error_return_item(42)),
               vector{42});
      check_eq(collect(
                 build(range(1, 2).concat(obs_error()).concat(range(1, 2)))
                   .on_error_return_item(42)),
               vector{1, 2, 42});
      check_eq(collect(
                 build(range(1, 3).concat(obs_error()).concat(range(1, 3)))
                   .on_error_return_item(42)),
               vector{1, 2, 3, 42});
      check_eq(collect(build(range(1, 2).concat(obs_error()))
                         .on_error_return_item(42)
                         .take(2)),
               vector{1, 2});
      check_eq(collect(build(range(1, 3).concat(obs_error()))
                         .on_error_return_item(42)
                         .take(3)),
               vector{1, 2, 3});
    }
  }
}

TEST("on_error_return() replaces an error with a value") {
  SECTION("on_error_return() does nothing if no error occurs") {
    SECTION("blueprint") {
      check_eq(collect(range(1, 1).on_error_return(
                 [](const error&) { return expected<int>{42}; })),
               vector{1});
      check_eq(collect(range(1, 2).on_error_return(
                 [](const error&) { return expected<int>{42}; })),
               vector{1, 2});
      check_eq(collect(range(1, 3).on_error_return(
                 [](const error&) { return expected<int>{42}; })),
               vector{1, 2, 3});
      check_eq(collect(range(1, 0).on_error_return(
                 [](const error&) { return expected<int>{42}; })),
               nil);
    }
    SECTION("observable") {
      check_eq(collect(build(range(1, 1)).on_error_return([](const error&) {
                 return expected<int>{42};
               })),
               vector{1});
      check_eq(collect(build(range(1, 2)).on_error_return([](const error&) {
                 return expected<int>{42};
               })),
               vector{1, 2});
      check_eq(collect(build(range(1, 3)).on_error_return([](const error&) {
                 return expected<int>{42};
               })),
               vector{1, 2, 3});
      check_eq(collect(build(range(1, 0)).on_error_return([](const error&) {
                 return expected<int>{42};
               })),
               nil);
    }
  }
  SECTION("on_error_return() replaces an error with a value from the handler") {
    auto return_42 = [](const error&) { return expected<int>{42}; };
    SECTION("blueprint") {
      check_eq(collect(
                 range(1, 0).concat(obs_error()).on_error_return(return_42)),
               vector{42});
      check_eq(collect(range(1, 2)
                         .concat(obs_error())
                         .concat(range(1, 2))
                         .on_error_return(return_42)),
               vector{1, 2, 42});
      check_eq(collect(range(1, 3)
                         .concat(obs_error())
                         .concat(range(1, 3))
                         .on_error_return(return_42)),
               vector{1, 2, 3, 42});
      check_eq(
        collect(
          range(1, 2).concat(obs_error()).on_error_return(return_42).take(2)),
        vector{1, 2});
      check_eq(
        collect(
          range(1, 3).concat(obs_error()).on_error_return(return_42).take(3)),
        vector{1, 2, 3});
    }
    SECTION("observable") {
      check_eq(
        collect(
          build(range(1, 0).concat(obs_error())).on_error_return(return_42)),
        vector{42});
      check_eq(collect(
                 build(range(1, 2).concat(obs_error()).concat(range(1, 2)))
                   .on_error_return(return_42)),
               vector{1, 2, 42});
      check_eq(collect(
                 build(range(1, 3).concat(obs_error()).concat(range(1, 3)))
                   .on_error_return(return_42)),
               vector{1, 2, 3, 42});
      check_eq(collect(build(range(1, 2).concat(obs_error()))
                         .on_error_return(return_42)
                         .take(2)),
               vector{1, 2});
      check_eq(collect(build(range(1, 3).concat(obs_error()))
                         .on_error_return(return_42)
                         .take(3)),
               vector{1, 2, 3});
    }
  }
  SECTION("on_error_return() forwards errors from the handler") {
    auto return_unexpected = [](const error&) {
      return expected<int>{make_error(sec::unexpected_message)};
    };
    auto return_err = [](const error& err) { return expected<int>{err}; };
    SECTION("blueprint") {
      check_eq(collect(obs_error().on_error_return(return_unexpected)),
               sec::unexpected_message);
      check_eq(collect(range(1, 2)
                         .concat(obs_error())
                         .concat(range(1, 2))
                         .on_error_return(return_unexpected)),
               sec::unexpected_message);
      check_eq(collect(range(1, 3)
                         .concat(obs_error())
                         .concat(range(1, 3))
                         .on_error_return(return_err)),
               sec::runtime_error);
      check_eq(collect(
                 range(1, 2).concat(obs_error()).on_error_return(return_err)),
               sec::runtime_error);
      check_eq(
        collect(
          range(1, 3).concat(obs_error()).on_error_return(return_unexpected)),
        sec::unexpected_message);
    }
    SECTION("observable") {
      check_eq(collect(build(obs_error()).on_error_return(return_unexpected)),
               sec::unexpected_message);
      check_eq(collect(
                 build(range(1, 2).concat(obs_error()).concat(range(1, 2)))
                   .on_error_return(return_unexpected)),
               sec::unexpected_message);
      check_eq(collect(
                 build(range(1, 3).concat(obs_error()).concat(range(1, 3)))
                   .on_error_return(return_err)),
               sec::runtime_error);
      check_eq(
        collect(
          build(range(1, 2).concat(obs_error())).on_error_return(return_err)),
        sec::runtime_error);
      check_eq(collect(build(range(1, 3).concat(obs_error()))
                         .on_error_return(return_unexpected)),
               sec::unexpected_message);
    }
  }
}

} // WITH_FIXTURE(test::fixture::flow)

} // namespace
