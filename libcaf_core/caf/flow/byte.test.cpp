// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

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

} // namespace

WITH_FIXTURE(test::fixture::flow) {

SCENARIO("to_chunks splits a sequence of bytes into chunks") {
  auto input = to_bytes("Sample string"s);
  GIVEN("an observable<byte>") {
    WHEN("transforming the input with to_chunks") {
      THEN("bytes are separated into chunks") {
        std::vector<chunk> output{};
        make_observable()
          .from_container(input)
          .transform(caf::flow::byte::to_chunks(5))
          .for_each([&output](const chunk& x) { output.push_back(x); });
        run_flows();
        if (check_eq(output.size(), 3u)) {
          check(output[0].equal_to(chunk{byte_span{input}.subspan(0, 5)}));
          check(output[1].equal_to(chunk{byte_span{input}.subspan(5, 5)}));
          check(output[2].equal_to(chunk{byte_span{input}.subspan(10)}));
        }
      }
    }
    WHEN("canceling the input observable before the all items are emitted") {
      THEN("the observer only receives a subset of the values") {
        std::vector<chunk> output{};
        make_observable()
          .from_container(input)
          .transform(caf::flow::byte::to_chunks(5))
          .take(2)
          .for_each([&output](const chunk& x) { output.push_back(x); });
        run_flows();
        if (check_eq(output.size(), 2u)) {
          check(output[0].equal_to(chunk{byte_span{input}.subspan(0, 5)}));
          check(output[1].equal_to(chunk{byte_span{input}.subspan(5, 5)}));
        }
      }
    }
  }
  GIVEN("an observable<byte> that emits an error") {
    WHEN("transforming the input with to_chunks") {
      THEN("the observer receives an error and no items") {
        auto obs = make_observable();
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
  }
  GIVEN("an observable<byte> that emits an error after emitting some values") {
    WHEN("transforming the input with to_chunks") {
      THEN("bytes are separated into chunks until the error occurs") {
        auto obs = make_observable();
        caf::error result;
        std::vector<chunk> output{};
        obs.from_container(input)
          .concat(obs.fail<std::byte>(make_error(caf::sec::runtime_error)))
          .transform(caf::flow::byte::to_chunks(5))
          .do_on_error([&result](const error& what) { result = what; })
          .for_each([&output](const chunk& x) { output.push_back(x); });
        run_flows();
        if (check_eq(output.size(), 3u)) {
          check(output[0].equal_to(chunk{byte_span{input}.subspan(0, 5)}));
          check(output[1].equal_to(chunk{byte_span{input}.subspan(5, 5)}));
          check(output[2].equal_to(chunk{byte_span{input}.subspan(10)}));
        }
        check_eq(result, sec::runtime_error);
      }
    }
    WHEN("canceling the input observable before the error occurs") {
      THEN("the observer only receives values and on_error is not called") {
        auto obs = make_observable();
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
          check(output[0].equal_to(chunk{byte_span{input}.subspan(0, 5)}));
          check(output[1].equal_to(chunk{byte_span{input}.subspan(5, 5)}));
          check(output[2].equal_to(chunk{byte_span{input}.subspan(10)}));
        }
        check_eq(result, sec::none);
      }
    }
  }
}

SCENARIO("split_at splits a sequence of bytes into chunks") {
  auto input = to_bytes("Sample string"s);
  GIVEN("an observable<byte>") {
    WHEN("transforming the input with split_at") {
      THEN("bytes are separated into chunks") {
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
    WHEN("canceling the input observable before the all items are emitted") {
      THEN("the observer only receives a subset of the values") {
        std::vector<chunk> output{};
        make_observable()
          .from_container(input)
          .transform(caf::flow::byte::split_at(std::byte{' '}))
          .take(1)
          .for_each([&output](const chunk& x) { output.push_back(x); });
        run_flows();
        if (check_eq(output.size(), 1u)) {
          check(output[0].equal_to(chunk{to_bytes("Sample"s)}));
        }
      }
    }
  }
  GIVEN("an observable<byte> that emits an error") {
    WHEN("transforming the input with to_chunks") {
      THEN("the observer receives an error and no items") {
        auto obs = make_observable();
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
  }
  GIVEN("an observable<byte> that emits an error after emitting some values") {
    WHEN("transforming the input with to_chunks") {
      THEN("bytes are separated into chunks until the error occurs") {
        auto obs = make_observable();
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
    WHEN("canceling the input observable before the error occurs") {
      THEN("the observer only receives values and on_error is not called") {
        auto obs = make_observable();
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
        check_eq(result, sec::none);
      }
    }
  }
}

SCENARIO("split_as_utf8_at splits a sequence of bytes into strings") {
  GIVEN("an observable<byte>") {
    WHEN("transforming the input with split_as_utf8_at") {
      THEN("bytes are converted into strings") {
        auto input = to_bytes("Sample string"s);
        std::vector<cow_string> output{};
        make_observable()
          .from_container(input)
          .transform(caf::flow::byte::split_as_utf8_at(' '))
          .for_each([&output](const cow_string& x) { output.push_back(x); });
        run_flows();
        if (check_eq(output.size(), 2u)) {
          check_eq(output[0], "Sample"s);
          check_eq(output[1], "string"s);
        }
      }
    }
    WHEN("canceling the input observable before the all items are emitted") {
      THEN("the observer only receives a subset of the values") {
        auto input = to_bytes("very long sample string"s);
        std::vector<cow_string> output{};
        make_observable()
          .from_container(input)
          .transform(caf::flow::byte::split_as_utf8_at(' '))
          .take(2)
          .for_each([&output](const cow_string& x) { output.push_back(x); });
        run_flows();
        check_eq(output.size(), 2u);
        if (check_eq(output.size(), 2u)) {
          check_eq(output[0], "very"s);
          check_eq(output[1], "long"s);
        }
      }
    }
  }
  GIVEN("an observable<byte> that emits an error") {
    WHEN("transforming the input with split_as_utf8_at") {
      THEN("the observer receives an error and no items") {
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
  }
  GIVEN("an observable<byte> that emits an error after emitting some values") {
    WHEN("transforming the input with split_as_utf8_at") {
      THEN("bytes are separated into strings until the error occurs") {
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
          check_eq(output[0], "Sample"s);
          check_eq(output[1], "string"s);
        }
        check_eq(result, sec::runtime_error);
      }
    }
    WHEN("canceling the input observable before the error occurs") {
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
          check_eq(output[0], "Sample"s);
          check_eq(output[1], "string"s);
        }
        check_ne(result, sec::runtime_error);
      }
    }
  }
  GIVEN("an observable<byte> that consists only of separators") {
    auto input = to_bytes("   "s);
    WHEN("transforming the input with split_as_utf8_at") {
      THEN("the observer receives empty strings") {
        std::vector<cow_string> output{};
        make_observable()
          .from_container(input)
          .transform(caf::flow::byte::split_as_utf8_at(' '))
          .for_each([&output](const cow_string& x) { output.push_back(x); });
        run_flows();
        if (check_eq(output.size(), 3u)) {
          check_eq(output[0], ""s);
          check_eq(output[1], ""s);
          check_eq(output[2], ""s);
        }
      }
    }
  }
}

SCENARIO("split_as_utf8_at rejects invalid UTF-8 inputs") {
  GIVEN("an observable<byte>") {
    auto input = to_bytes("Sample "s);
    WHEN("transforming the input with split_as_utf8_at") {
      THEN("the operator stops at the first invalid UTF-8 sequence") {
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
          check_eq(output[0], "Sample"s);
        }
        check_eq(result, sec::invalid_utf8);
      }
    }
  }
}

} // WITH_FIXTURE(test::fixture::flow)
