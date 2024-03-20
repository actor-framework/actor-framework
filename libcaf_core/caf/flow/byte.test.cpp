// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/flow/byte.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/flow/multicaster.hpp"

using namespace caf;
using namespace std::literals;

namespace {

template <class T>
auto to_bytes(const T& data) {
  std::vector<std::byte> byte_arr;
  std::transform(data.begin(), data.end(), std::back_inserter(byte_arr),
                 [](auto element) { return static_cast<std::byte>(element); });
  return byte_arr;
}

WITH_FIXTURE(test::fixture::flow) {

SCENARIO("to_chunks splits a sequence of bytes into chunks") {
  GIVEN("an observable<byte>") {
    auto input = to_bytes("Sample string"s);
    WHEN("transforming the input with to_chunks") {
      THEN("all values from container is received") {
        std::vector<chunk> output{};
        make_observable()
          .from_container(input)
          .transform(caf::flow::byte::to_chunks(5))
          .for_each([&output](const chunk& x) { output.push_back(x); });
        run_flows();
        if (check_eq(output.size(), 3u)) {
          check(output[0].equal_to(chunk{to_bytes("Sampl"s)}));
          check(output[1].equal_to(chunk{to_bytes("e str"s)}));
          check(output[2].equal_to(chunk{to_bytes("ing"s)}));
        }
      }
    }
    WHEN("concatenating container with a fail observable") {
      THEN("the observer receives all values and error") {
        auto obs = make_observable();
        auto input = to_bytes("Sample string"s);
        caf::error result;
        std::vector<chunk> output{};
        obs.from_container(input)
          .concat(obs.fail<std::byte>(make_error(caf::sec::runtime_error)))
          .transform(caf::flow::byte::to_chunks(5))
          .do_on_error([&result](const error& what) { result = what; })
          .for_each([&output](const chunk& x) { output.push_back(x); });
        run_flows();
        if (check_eq(output.size(), 3u)) {
          check(output[0].equal_to(chunk{to_bytes("Sampl"s)}));
          check(output[1].equal_to(chunk{to_bytes("e str"s)}));
          check(output[2].equal_to(chunk{to_bytes("ing"s)}));
        }
        check_eq(result, sec::runtime_error);
      }
    }
    WHEN("concatenating fail observable with container") {
      THEN("the observer receives only error") {
        auto obs = make_observable();
        auto input = to_bytes("Sample string"s);
        caf::error result;
        std::vector<chunk> output{};
        obs.fail<std::byte>(make_error(caf::sec::runtime_error))
          .concat(obs.from_container(input))
          .transform(caf::flow::byte::to_chunks(5))
          .do_on_error([&result](const error& what) { result = what; })
          .for_each([&output](const chunk& x) { output.push_back(x); });
        run_flows();
        check_eq(output.size(), 0u);
        check_eq(result, sec::runtime_error);
      }
    }
    WHEN("on_next returns false before calling on_error") {
      THEN("the observer only receives values and on_error is not called") {
        auto obs = make_observable();
        auto input = to_bytes("Sample string"s);
        caf::error result;
        std::vector<chunk> output{};
        obs.from_container(input)
          .concat(obs.fail<std::byte>(make_error(caf::sec::runtime_error)))
          .transform(caf::flow::byte::to_chunks(5))
          .take(3)
          .do_on_error([&result](const error& what) { result = what; })
          .for_each([&output](const chunk& x) { output.push_back(x); });
        run_flows();
        if (check_eq(output.size(), 3u)) {
          check(output[0].equal_to(chunk{to_bytes("Sampl"s)}));
          check(output[1].equal_to(chunk{to_bytes("e str"s)}));
          check(output[2].equal_to(chunk{to_bytes("ing"s)}));
        }
        check_ne(result, sec::runtime_error);
      }
    }
    WHEN("on_next returns false before calling on_complete") {
      THEN("the observer receives values and to_chunks does not call "
           "on_complete") {
        std::vector<chunk> output{};
        make_observable()
          .from_container(input)
          .transform(caf::flow::byte::to_chunks(5))
          .take(3)
          .for_each([&output](const chunk& x) { output.push_back(x); });
        run_flows();
        if (check_eq(output.size(), 3u)) {
          check(output[0].equal_to(chunk{to_bytes("Sampl"s)}));
          check(output[1].equal_to(chunk{to_bytes("e str"s)}));
          check(output[2].equal_to(chunk{to_bytes("ing"s)}));
        }
      }
    }
  }
}

SCENARIO("split_at splits a sequence of bytes into chunks on separator") {
  GIVEN("an observable<byte>") {
    auto input = to_bytes("Sample string"s);
    WHEN("transforming the input with split_at") {
      THEN("all values from container is received") {
        std::vector<chunk> output{};
        make_observable()
          .from_container(input)
          .transform(caf::flow::byte::split_at(std::byte{' '}))
          .for_each([&output](const chunk& x) { output.push_back(x); });
        run_flows();
        if (check_eq(output.size(), 2u)) {
          check(output[0].equal_to(chunk{to_bytes("Sample"s)}));
          check(output[1].equal_to(chunk{to_bytes("string"s)}));
        }
      }
    }
    WHEN("concatenating container with a fail observable") {
      THEN("the observer receives all values and error") {
        auto obs = make_observable();
        auto input = to_bytes("Sample string"s);
        caf::error result;
        std::vector<chunk> output{};
        obs.from_container(input)
          .concat(obs.fail<std::byte>(make_error(caf::sec::runtime_error)))
          .transform(caf::flow::byte::split_at(std::byte{' '}))
          .do_on_error([&result](const error& what) { result = what; })
          .for_each([&output](const chunk& x) { output.push_back(x); });
        run_flows();
        if (check_eq(output.size(), 2u)) {
          check(output[0].equal_to(chunk{to_bytes("Sample"s)}));
          check(output[1].equal_to(chunk{to_bytes("string"s)}));
        }
        check_eq(result, sec::runtime_error);
      }
    }
    WHEN("concatenating fail observable with container") {
      THEN("the observer receives only error") {
        auto obs = make_observable();
        auto input = to_bytes("Sample string"s);
        caf::error result;
        std::vector<chunk> output{};
        obs.fail<std::byte>(make_error(caf::sec::runtime_error))
          .concat(obs.from_container(input))
          .transform(caf::flow::byte::split_at(std::byte{' '}))
          .do_on_error([&result](const error& what) { result = what; })
          .for_each([&output](const chunk& x) { output.push_back(x); });
        run_flows();
        check_eq(output.size(), 0u);
        check_eq(result, sec::runtime_error);
      }
    }
    WHEN("on_next returns false before calling on_error") {
      THEN("the observer only receives values and on_error is not called") {
        auto obs = make_observable();
        auto input = to_bytes("Sample string"s);
        caf::error result;
        std::vector<chunk> output{};
        obs.from_container(input)
          .concat(obs.fail<std::byte>(make_error(caf::sec::runtime_error)))
          .transform(caf::flow::byte::split_at(std::byte{' '}))
          .take(2)
          .do_on_error([&result](const error& what) { result = what; })
          .for_each([&output](const chunk& x) { output.push_back(x); });
        run_flows();
        if (check_eq(output.size(), 2u)) {
          check(output[0].equal_to(chunk{to_bytes("Sample"s)}));
          check(output[1].equal_to(chunk{to_bytes("string"s)}));
        }
        check_ne(result, sec::runtime_error);
      }
    }
    WHEN("on_next returns false before calling on_complete") {
      THEN(
        "the observer receives values and split_at does not call on_complete") {
        std::vector<chunk> output{};
        make_observable()
          .from_container(input)
          .transform(caf::flow::byte::split_at(std::byte{' '}))
          .take(2)
          .for_each([&output](const chunk& x) { output.push_back(x); });
        run_flows();
        check_eq(output.size(), 2u);
        if (check_eq(output.size(), 2u)) {
          check(output[0].equal_to(chunk{to_bytes("Sample"s)}));
          check(output[1].equal_to(chunk{to_bytes("string"s)}));
        }
      }
    }
  }
}

SCENARIO(
  "split_as_utf8_at splits a sequence of bytes into cow_string on separator") {
  GIVEN("an observable<byte>") {
    auto input = to_bytes("Sample string"s);
    WHEN("transforming the input with split_as_utf8_at") {
      THEN("all values from container is received") {
        std::vector<cow_string> output{};
        make_observable()
          .from_container(input)
          .transform(caf::flow::byte::split_as_utf8_at(' '))
          .for_each([&output](const cow_string& x) { output.push_back(x); });
        run_flows();
        if (check_eq(output.size(), 2u)) {
          check_eq(output[0], cow_string{"Sample"s});
          check_eq(output[1], cow_string{"string"s});
        }
      }
    }
    WHEN("concatenating container with a fail observable") {
      THEN("the observer receives all values and error") {
        auto obs = make_observable();
        auto input = to_bytes("Sample string"s);
        caf::error result;
        std::vector<cow_string> output{};
        obs.from_container(input)
          .concat(obs.fail<std::byte>(make_error(caf::sec::runtime_error)))
          .transform(caf::flow::byte::split_as_utf8_at(' '))
          .do_on_error([&result](const error& what) { result = what; })
          .for_each([&output](const cow_string& x) { output.push_back(x); });
        run_flows();
        if (check_eq(output.size(), 2u)) {
          check_eq(output[0], cow_string{"Sample"s});
          check_eq(output[1], cow_string{"string"s});
        }
        check_eq(result, sec::runtime_error);
      }
    }
    WHEN("concatenating fail observable with container") {
      THEN("the observer receives only error") {
        auto obs = make_observable();
        auto input = to_bytes("Sample string"s);
        caf::error result;
        std::vector<cow_string> output{};
        obs.fail<std::byte>(make_error(caf::sec::runtime_error))
          .concat(obs.from_container(input))
          .transform(caf::flow::byte::split_as_utf8_at(' '))
          .do_on_error([&result](const error& what) { result = what; })
          .for_each([&output](const cow_string& x) { output.push_back(x); });
        run_flows();
        check_eq(output.size(), 0u);
        check_eq(result, sec::runtime_error);
      }
    }
    WHEN("on_next returns false before calling on_error") {
      THEN("the observer only receives values and on_error is not called") {
        auto obs = make_observable();
        auto input = to_bytes("Sample string"s);
        caf::error result;
        std::vector<cow_string> output{};
        obs.from_container(input)
          .concat(obs.fail<std::byte>(make_error(caf::sec::runtime_error)))
          .transform(caf::flow::byte::split_as_utf8_at(' '))
          .take(2)
          .do_on_error([&result](const error& what) { result = what; })
          .for_each([&output](const cow_string& x) { output.push_back(x); });
        run_flows();
        if (check_eq(output.size(), 2u)) {
          check_eq(output[0], cow_string{"Sample"s});
          check_eq(output[1], cow_string{"string"s});
        }
        check_ne(result, sec::runtime_error);
      }
    }
    WHEN("on_next returns false before calling on_complete") {
      THEN("the observer receives values and split_as_utf8_at does not call "
           "on_complete") {
        std::vector<cow_string> output{};
        make_observable()
          .from_container(input)
          .transform(caf::flow::byte::split_as_utf8_at(' '))
          .take(2)
          .for_each([&output](const cow_string& x) { output.push_back(x); });
        run_flows();
        check_eq(output.size(), 2u);
        if (check_eq(output.size(), 2u)) {
          check_eq(output[0], cow_string{"Sample"s});
          check_eq(output[1], cow_string{"string"s});
        }
      }
    }
  }
  GIVEN("an observable<byte> with only separators") {
    auto input = to_bytes("   "s);
    WHEN("transforming the input with split_as_utf8_at") {
      THEN("empty cow_string is received for each separator") {
        std::vector<cow_string> output{};
        make_observable()
          .from_container(input)
          .transform(caf::flow::byte::split_as_utf8_at(' '))
          .for_each([&output](const cow_string& x) { output.push_back(x); });
        run_flows();
        if (check_eq(output.size(), 3u)) {
          check_eq(output[0], cow_string{""s});
          check_eq(output[1], cow_string{""s});
          check_eq(output[2], cow_string{""s});
        }
      }
    }
  }
}

SCENARIO("split_as_utf8_at can discard invalid utf-8 string") {
  GIVEN("an observable<byte>") {
    auto input = to_bytes("Sample "s);
    WHEN("transforming the input with split_as_utf8_at") {
      THEN("only valid values from container is received") {
        input.push_back(static_cast<std::byte>(0xc8));
        auto suffix = to_bytes(" string"s);
        caf::error result;
        input.insert(input.end(), suffix.begin(), suffix.end());
        std::vector<cow_string> output{};
        make_observable()
          .from_container(input)
          .transform(caf::flow::byte::split_as_utf8_at(' '))
          .do_on_error([&result](const error& what) { result = what; })
          .for_each([&output](const cow_string& x) { output.push_back(x); });
        run_flows();
        if (check_eq(output.size(), 1u)) {
          check_eq(output[0], cow_string{"Sample"s});
        }
        check_eq(result, sec::invalid_utf8);
      }
    }
  }
}

} // WITH_FIXTURE(test::fixture::flow)

} // namespace
