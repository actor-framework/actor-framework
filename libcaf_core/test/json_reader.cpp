// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE json_reader

#include "caf/json_reader.hpp"

#include "core-test.hpp"

#include "caf/dictionary.hpp"

using namespace caf;

namespace {

struct fixture {
  template <class T>
  void add_test_case(string_view input, T val) {
    auto f = [this, input, obj{std::move(val)}]() -> bool {
      auto tmp = T{};
      auto res = CHECK(reader.load(input))    // parse JSON
                 && CHECK(reader.apply(tmp)); // deserialize object
      if (res) {
        if constexpr (std::is_same<T, message>::value)
          res = CHECK_EQ(to_string(tmp), to_string(obj));
        else
          res = CHECK_EQ(tmp, obj);
      }
      if (!res)
        MESSAGE("rejected input: " << input);
      return res;
    };
    test_cases.emplace_back(std::move(f));
  }

  template <class T, class... Ts>
  std::vector<T> ls(Ts... xs) {
    return {std::move(xs)...};
  }

  template <class T>
  using dict = dictionary<T>;

  fixture();

  json_reader reader;

  std::vector<std::function<bool()>> test_cases;
};

fixture::fixture() {
  add_test_case(R"_(true)_", true);
  add_test_case(R"_(false)_", false);
  add_test_case(R"_([true, false])_", ls<bool>(true, false));
  add_test_case(R"_(42)_", int32_t{42});
  add_test_case(R"_([1, 2, 3])_", ls<int32_t>(1, 2, 3));
  add_test_case(R"_(2.0)_", 2.0);
  add_test_case(R"_([2.0, 4.0, 8.0])_", ls<double>(2.0, 4.0, 8.0));
  add_test_case(R"_("hello \"world\"!")_", std::string{R"_(hello "world"!)_"});
  add_test_case(R"_(["hello", "world"])_", ls<std::string>("hello", "world"));
  add_test_case(R"_({"a": 1, "b": 2})_", my_request(1, 2));
  add_test_case(R"_({"a": 1, "b": 2})_", dict<int>({{"a", 1}, {"b", 2}}));
  add_test_case(R"_([{"@type": "my_request", "a": 1, "b": 2}])_",
                make_message(my_request(1, 2)));
}

} // namespace

CAF_TEST_FIXTURE_SCOPE(json_reader_tests, fixture)

CAF_TEST(json baselines) {
  size_t baseline_index = 0;
  detail::monotonic_buffer_resource resource;
  for (auto& f : test_cases) {
    MESSAGE("test case at index " << baseline_index++);
    if (!f())
      if (auto reason = reader.get_error())
        MESSAGE("JSON reader stopped due to: " << reason);
  }
}

CAF_TEST_FIXTURE_SCOPE_END()
