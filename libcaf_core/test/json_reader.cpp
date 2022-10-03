// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE json_reader

#include "caf/json_reader.hpp"

#include "core-test.hpp"

#include "caf/dictionary.hpp"

using namespace caf;

using namespace std::literals;

namespace {

struct fixture {
  template <class T>
  void add_test_case(std::string_view input, T val) {
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
    std::vector<T> result;
    (result.emplace_back(std::move(xs)), ...);
    return result;
  }

  template <class T, class... Ts>
  std::set<T> set(Ts... xs) {
    std::set<T> result;
    (result.emplace(std::move(xs)), ...);
    return result;
  }

  template <class T>
  using dict = dictionary<T>;

  fixture();

  json_reader reader;

  std::vector<std::function<bool()>> test_cases;
};

fixture::fixture() {
  using i32_list = std::vector<int32_t>;
  using str_list = std::vector<std::string>;
  using str_set = std::set<std::string>;
  add_test_case(R"_(true)_", true);
  add_test_case(R"_(false)_", false);
  add_test_case(R"_([true, false])_", ls<bool>(true, false));
  add_test_case(R"_(42)_", int32_t{42});
  add_test_case(R"_([1, 2, 3])_", ls<int32_t>(1, 2, 3));
  add_test_case(R"_([[1, 2], [3], []])_",
                ls<i32_list>(ls<int32_t>(1, 2), ls<int32_t>(3), ls<int32_t>()));
  add_test_case(R"_(2.0)_", 2.0);
  add_test_case(R"_([2.0, 4.0, 8.0])_", ls<double>(2.0, 4.0, 8.0));
  add_test_case(R"_("hello \"world\"!")_", std::string{R"_(hello "world"!)_"});
  add_test_case(R"_(["hello", "world"])_", ls<std::string>("hello", "world"));
  add_test_case(R"_(["hello", "world"])_", set<std::string>("hello", "world"));
  add_test_case(R"_({"a": 1, "b": 2})_", my_request(1, 2));
  add_test_case(R"_({"a": 1, "b": 2})_", dict<int>({{"a", 1}, {"b", 2}}));
  add_test_case(R"_({"xs": ["x1", "x2"], "ys": ["y1", "y2"]})_",
                dict<str_list>({{"xs", ls<std::string>("x1", "x2")},
                                {"ys", ls<std::string>("y1", "y2")}}));
  add_test_case(R"_({"xs": ["x1", "x2"], "ys": ["y1", "y2"]})_",
                dict<str_set>({{"xs", set<std::string>("x1", "x2")},
                               {"ys", set<std::string>("y1", "y2")}}));
  add_test_case(R"_([{"@type": "my_request", "a": 1, "b": 2}])_",
                make_message(my_request(1, 2)));
  add_test_case(
    R"_({"top-left":{"x":100,"y":200},"bottom-right":{"x":10,"y":20}})_",
    rectangle{{100, 200}, {10, 20}});
  add_test_case(R"({"@type": "phone_book",)"
                R"( "city": "Model City",)"
                R"( "entries": )"
                R"({"Bob": 5556837,)"
                R"( "Jon": 5559347}})",
                phone_book{"Model City", {{"Bob", 5556837}, {"Jon", 5559347}}});
  add_test_case(R"({"@type": "widget", )"
                R"("color": "red", )"
                R"("@shape-type": "circle", )"
                R"("shape": )"
                R"({"center": {"x": 15, "y": 15}, )"
                R"("radius": 5}})",
                widget{"red", circle{{15, 15}, 5}});
  add_test_case(R"({"@type": "widget", )"
                R"("color": "blue", )"
                R"("@shape-type": "rectangle", )"
                R"("shape": )"
                R"({"top-left": {"x": 10, "y": 10}, )"
                R"("bottom-right": {"x": 20, "y": 20}}})",
                widget{"blue", rectangle{{10, 10}, {20, 20}}});
}

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

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

SCENARIO("mappers enable custom type names in JSON input") {
  GIVEN("a custom mapper") {
    class custom_mapper : public type_id_mapper {
      std::string_view operator()(type_id_t type) const override {
        switch (type) {
          case type_id_v<std::string>:
            return "String";
          case type_id_v<int32_t>:
            return "Int";
          default:
            return query_type_name(type);
        }
      }
      type_id_t operator()(std::string_view name) const override {
        if (name == "String")
          return type_id_v<std::string>;
        else if (name == "Int")
          return type_id_v<int32_t>;
        else
          return query_type_id(name);
      }
    };
    custom_mapper mapper_instance;
    WHEN("reading a variant from JSON") {
      using value_type = std::variant<int32_t, std::string>;
      THEN("the custom mapper translates between external and internal names") {
        json_reader reader;
        reader.mapper(&mapper_instance);
        auto value = value_type{};
        auto input1 = R"_({"@value-type": "String", "value": "hello world"})_"s;
        if (CHECK(reader.load(input1))) {
          if (!CHECK(reader.apply(value)))
            MESSAGE("reader reported error: " << reader.get_error());
          if (CHECK(std::holds_alternative<std::string>(value)))
            CHECK_EQ(std::get<std::string>(value), "hello world"s);
        } else {
          MESSAGE("reader reported error: " << reader.get_error());
        }
        reader.reset();
        auto input2 = R"_({"@value-type": "Int", "value": 42})_"sv;
        if (CHECK(reader.load(input2))) {
          if (!CHECK(reader.apply(value)))
            MESSAGE("reader reported error: " << reader.get_error());
          if (CHECK(std::holds_alternative<int32_t>(value)))
            CHECK_EQ(std::get<int32_t>(value), 42);
        } else {
          MESSAGE("reader reported error: " << reader.get_error());
        }
      }
    }
  }
}

END_FIXTURE_SCOPE()
