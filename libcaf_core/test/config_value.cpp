/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE config_value

#include "caf/config_value.hpp"

#include "core-test.hpp"
#include "nasty.hpp"

#include <cmath>
#include <list>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/deep_to_string.hpp"
#include "caf/detail/bounds_checker.hpp"
#include "caf/make_config_option.hpp"
#include "caf/none.hpp"
#include "caf/pec.hpp"
#include "caf/string_view.hpp"
#include "caf/variant.hpp"

#include "caf/detail/bounds_checker.hpp"
#include "caf/detail/overload.hpp"
#include "caf/detail/parse.hpp"

using std::string;

using namespace caf;

using namespace std::string_literals;

namespace {

using list = config_value::list;

using dictionary = config_value::dictionary;

struct dictionary_builder {
  dictionary dict;

  template <class T>
  dictionary_builder&& add(string_view key, T&& value) && {
    dict.emplace(key, config_value{std::forward<T>(value)});
    return std::move(*this);
  }

  dictionary make() && {
    return std::move(dict);
  }

  config_value make_cv() && {
    return config_value{std::move(dict)};
  }
};

dictionary_builder dict() {
  return {};
}

template <class... Ts>
config_value cfg_lst(Ts&&... xs) {
  config_value::list lst{config_value{std::forward<Ts>(xs)}...};
  return config_value{std::move(lst)};
}

struct fixture {
  config_value cv_null;
  config_value cv_true;
  config_value cv_false;
  config_value cv_empty_uri;
  config_value cv_empty_list;
  config_value cv_empty_dict;
  config_value cv_caf_uri;

  fixture()
    : cv_true(true),
      cv_false(false),
      cv_empty_uri(uri{}),
      cv_empty_list(config_value::list{}),
      cv_empty_dict(config_value::dictionary{}) {
    cv_caf_uri = unbox(make_uri("https://actor-framework.org"));
  }
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(config_value_tests, fixture)

SCENARIO("get_as can convert config values to boolean") {
  GIVEN("a config value x with value true or false") {
    WHEN("using get_as with bool") {
      THEN("conversion succeeds") {
        CHECK_EQ(get_as<bool>(cv_true), true);
        CHECK_EQ(get_as<bool>(cv_false), false);
      }
    }
  }
  GIVEN("a config value x with value \"true\" or \"false\"") {
    WHEN("using get_as with bool") {
      THEN("conversion succeeds") {
        CHECK_EQ(get_as<bool>(config_value{"true"s}), true);
        CHECK_EQ(get_as<bool>(config_value{"false"s}), false);
      }
    }
  }
  GIVEN("non-boolean config_values") {
    WHEN("using get_as with bool") {
      THEN("conversion fails") {
        CHECK_EQ(get_as<bool>(cv_null), sec::conversion_failed);
        CHECK_EQ(get_as<bool>(cv_empty_uri), sec::conversion_failed);
        CHECK_EQ(get_as<bool>(cv_empty_list), sec::conversion_failed);
        CHECK_EQ(get_as<bool>(cv_empty_dict), sec::conversion_failed);
        CHECK_EQ(get_as<bool>(config_value{0}), sec::conversion_failed);
        CHECK_EQ(get_as<bool>(config_value{1}), sec::conversion_failed);
        CHECK_EQ(get_as<bool>(config_value{0.f}), sec::conversion_failed);
        CHECK_EQ(get_as<bool>(config_value{1.f}), sec::conversion_failed);
        CHECK_EQ(get_as<bool>(config_value{""s}), sec::conversion_failed);
        CHECK_EQ(get_as<bool>(config_value{"1"s}), sec::conversion_failed);
      }
    }
  }
}

SCENARIO("get_as can convert config values to integers") {
  GIVEN("a config value x with value 32,768") {
    auto x = config_value{32'768};
    WHEN("using get_as with integer types") {
      THEN("conversion fails if bounds checks fail") {
        CHECK_EQ(get_as<uint64_t>(x), 32'768u);
        CHECK_EQ(get_as<int64_t>(x), 32'768);
        CHECK_EQ(get_as<uint32_t>(x), 32'768u);
        CHECK_EQ(get_as<int32_t>(x), 32'768);
        CHECK_EQ(get_as<uint16_t>(x), 32'768u);
        CHECK_EQ(get_as<int16_t>(x), sec::conversion_failed);
        CHECK_EQ(get_as<uint8_t>(x), sec::conversion_failed);
        CHECK_EQ(get_as<int8_t>(x), sec::conversion_failed);
      }
    }
  }
  GIVEN("a config value x with value -5") {
    auto x = config_value{-5};
    WHEN("using get_as with integer types") {
      THEN("conversion fails for all unsigned types") {
        CAF_CHECK_EQUAL(get_as<uint64_t>(x), sec::conversion_failed);
        CAF_CHECK_EQUAL(get_as<int64_t>(x), -5);
        CAF_CHECK_EQUAL(get_as<uint32_t>(x), sec::conversion_failed);
        CAF_CHECK_EQUAL(get_as<int32_t>(x), -5);
        CAF_CHECK_EQUAL(get_as<uint16_t>(x), sec::conversion_failed);
        CAF_CHECK_EQUAL(get_as<int16_t>(x), -5);
        CAF_CHECK_EQUAL(get_as<uint8_t>(x), sec::conversion_failed);
        CAF_CHECK_EQUAL(get_as<int8_t>(x), -5);
      }
    }
  }
  GIVEN("a config value x with value \"50000\"") {
    auto x = config_value{"50000"s};
    WHEN("using get_as with integer types") {
      THEN("CAF parses the string and performs a bound check") {
        CAF_CHECK_EQUAL(get_as<uint64_t>(x), 50'000u);
        CAF_CHECK_EQUAL(get_as<int64_t>(x), 50'000);
        CAF_CHECK_EQUAL(get_as<uint32_t>(x), 50'000u);
        CAF_CHECK_EQUAL(get_as<int32_t>(x), 50'000);
        CAF_CHECK_EQUAL(get_as<uint16_t>(x), 50'000u);
        CAF_CHECK_EQUAL(get_as<int16_t>(x), sec::conversion_failed);
        CAF_CHECK_EQUAL(get_as<uint8_t>(x), sec::conversion_failed);
        CAF_CHECK_EQUAL(get_as<int8_t>(x), sec::conversion_failed);
      }
    }
  }
  GIVEN("a config value x with value 50.0") {
    auto x = config_value{50.0};
    WHEN("using get_as with integer types") {
      THEN("CAF parses the string and performs a bound check") {
        CHECK_EQ(get_as<uint64_t>(x), 50u);
        CHECK_EQ(get_as<int64_t>(x), 50);
        CHECK_EQ(get_as<uint32_t>(x), 50u);
        CHECK_EQ(get_as<int32_t>(x), 50);
        CHECK_EQ(get_as<uint16_t>(x), 50u);
        CHECK_EQ(get_as<int16_t>(x), 50);
        CHECK_EQ(get_as<uint8_t>(x), 50u);
        CHECK_EQ(get_as<int8_t>(x), 50);
      }
    }
  }
  GIVEN("a config value x with value 50.05") {
    auto x = config_value{50.05};
    WHEN("using get_as with integer types") {
      THEN("CAF fails to convert the real to an integer") {
        CHECK_EQ(get_as<uint64_t>(x), sec::conversion_failed);
        CHECK_EQ(get_as<int64_t>(x), sec::conversion_failed);
        CHECK_EQ(get_as<uint32_t>(x), sec::conversion_failed);
        CHECK_EQ(get_as<int32_t>(x), sec::conversion_failed);
        CHECK_EQ(get_as<uint16_t>(x), sec::conversion_failed);
        CHECK_EQ(get_as<int16_t>(x), sec::conversion_failed);
        CHECK_EQ(get_as<uint8_t>(x), sec::conversion_failed);
        CHECK_EQ(get_as<int8_t>(x), sec::conversion_failed);
      }
    }
  }
  GIVEN("a config value x with value \"50.000\"") {
    auto x = config_value{"50.000"s};
    WHEN("using get_as with integer types") {
      THEN("CAF parses the string and performs a bound check") {
        CHECK_EQ(get_as<uint64_t>(x), 50u);
        CHECK_EQ(get_as<int64_t>(x), 50);
        CHECK_EQ(get_as<uint32_t>(x), 50u);
        CHECK_EQ(get_as<int32_t>(x), 50);
        CHECK_EQ(get_as<uint16_t>(x), 50u);
        CHECK_EQ(get_as<int16_t>(x), 50);
        CHECK_EQ(get_as<uint8_t>(x), 50u);
        CHECK_EQ(get_as<int8_t>(x), 50);
      }
    }
  }
  GIVEN("a config value x with value \"50.05\"") {
    auto x = config_value{"50.05"s};
    WHEN("using get_as with integer types") {
      THEN("CAF fails to convert the real to an integer") {
        CHECK_EQ(get_as<uint64_t>(x), sec::conversion_failed);
        CHECK_EQ(get_as<int64_t>(x), sec::conversion_failed);
        CHECK_EQ(get_as<uint32_t>(x), sec::conversion_failed);
        CHECK_EQ(get_as<int32_t>(x), sec::conversion_failed);
        CHECK_EQ(get_as<uint16_t>(x), sec::conversion_failed);
        CHECK_EQ(get_as<int16_t>(x), sec::conversion_failed);
        CHECK_EQ(get_as<uint8_t>(x), sec::conversion_failed);
        CHECK_EQ(get_as<int8_t>(x), sec::conversion_failed);
      }
    }
  }
  GIVEN("config_values of null, URI, boolean, list or dictionary") {
    WHEN("using get_as with integer types") {
      THEN("conversion fails") {
        CHECK_EQ(get_as<int64_t>(cv_null), sec::conversion_failed);
        CHECK_EQ(get_as<int64_t>(cv_true), sec::conversion_failed);
        CHECK_EQ(get_as<int64_t>(cv_false), sec::conversion_failed);
        CHECK_EQ(get_as<int64_t>(cv_empty_uri), sec::conversion_failed);
        CHECK_EQ(get_as<int64_t>(cv_empty_list), sec::conversion_failed);
        CHECK_EQ(get_as<int64_t>(cv_empty_dict), sec::conversion_failed);
      }
    }
  }
}

SCENARIO("get_as can convert config values to floating point numbers") {
  GIVEN("a config value x with value 1.79769e+308") {
    auto x = config_value{1.79769e+308};
    WHEN("using get_as with floating point types") {
      THEN("conversion fails if bounds checks fail") {
        CHECK_EQ(get_as<long double>(x), 1.79769e+308);
        CHECK_EQ(get_as<double>(x), 1.79769e+308);
        CHECK_EQ(get_as<float>(x), sec::conversion_failed);
      }
    }
  }
  GIVEN("a config value x with value \"3e7\"") {
    auto x = config_value{"3e7"s};
    WHEN("using get_as with floating point types") {
      THEN("CAF parses the string and converts the value") {
        CHECK_EQ(get_as<long double>(x), 3e7);
        CHECK_EQ(get_as<double>(x), 3e7);
        CHECK_EQ(get_as<float>(x), 3e7f);
      }
    }
  }
  GIVEN("a config value x with value 123") {
    auto x = config_value{123};
    WHEN("using get_as with floating point types") {
      THEN("CAF converts the value") {
        CHECK_EQ(get_as<long double>(x), 123.0);
        CHECK_EQ(get_as<double>(x), 123.0);
        CHECK_EQ(get_as<float>(x), 123.f);
      }
    }
  }
  GIVEN("config_values of null, URI, boolean, list or dictionary") {
    WHEN("using get_as with floating point types") {
      THEN("conversion fails") {
        CHECK_EQ(get_as<int64_t>(cv_null), sec::conversion_failed);
        CHECK_EQ(get_as<int64_t>(cv_true), sec::conversion_failed);
        CHECK_EQ(get_as<int64_t>(cv_false), sec::conversion_failed);
        CHECK_EQ(get_as<int64_t>(cv_empty_uri), sec::conversion_failed);
        CHECK_EQ(get_as<int64_t>(cv_empty_list), sec::conversion_failed);
        CHECK_EQ(get_as<int64_t>(cv_empty_dict), sec::conversion_failed);
      }
    }
  }
}

SCENARIO("get_as can convert config values to timespans") {
  using namespace std::chrono_literals;
  GIVEN("a config value with value 42s") {
    auto x = config_value{timespan{42s}};
    WHEN("using get_as with timespan") {
      THEN("conversion succeeds") {
        CHECK_EQ(get_as<timespan>(x), timespan{42s});
        CHECK_EQ(get_as<std::string>(x), "42s");
      }
    }
    WHEN("using get_as with type other than timespan or string") {
      THEN("conversion fails") {
        CHECK_EQ(get_as<int64_t>(x), sec::conversion_failed);
        CHECK_EQ(get_as<double>(x), sec::conversion_failed);
        // CHECK_EQ(get_as<uri>(x), sec::conversion_failed);
        CHECK_EQ(get_as<config_value::list>(x), sec::conversion_failed);
        CHECK_EQ(get_as<config_value::dictionary>(x), sec::conversion_failed);
      }
    }
  }
}

SCENARIO("get_as can convert config values to strings") {
  using string = std::string;
  GIVEN("any config value") {
    WHEN("using get_as with string") {
      THEN("CAF renders the value as string") {
        CHECK_EQ(get_as<string>(cv_null), "null");
        CHECK_EQ(get_as<string>(cv_true), "true");
        CHECK_EQ(get_as<string>(cv_false), "false");
        CHECK_EQ(get_as<string>(cv_empty_list), "[]");
        CHECK_EQ(get_as<string>(cv_empty_dict), "{}");
        CHECK_EQ(get_as<string>(config_value{42}), "42");
        CHECK_EQ(get_as<string>(config_value{4.2}), "4.2");
        CHECK_EQ(get_as<string>(config_value{timespan{4}}), "4ns");
        CHECK_EQ(get_as<string>(cv_caf_uri), "https://actor-framework.org");
      }
    }
  }
}

SCENARIO("get_as can convert config values to lists") {
  using list = config_value::list;
  GIVEN("a config value with value [1, 2, 3]") {
    auto x = make_config_value_list(1, 2, 3);
    WHEN("using get_as with config_value::list") {
      THEN("conversion succeeds") {
        auto maybe_res = get_as<list>(x);
        if (CHECK(maybe_res) && CHECK_EQ(maybe_res->size(), 3u)) {
          auto& res = *maybe_res;
          CHECK_EQ(get_as<int>(res[0]), 1);
          CHECK_EQ(get_as<int>(res[1]), 2);
          CHECK_EQ(get_as<int>(res[2]), 3);
        }
      }
    }
    WHEN("using get_as with vector<int>") {
      THEN("conversion succeeds") {
        auto maybe_res = get_as<std::vector<int>>(x);
        if (CHECK(maybe_res) && CHECK_EQ(maybe_res->size(), 3u)) {
          auto& res = *maybe_res;
          CHECK_EQ(res[0], 1);
          CHECK_EQ(res[1], 2);
          CHECK_EQ(res[2], 3);
        }
      }
    }
  }
  GIVEN("a config value with value \"[1, 2, 3]\"") {
    auto x = config_value("[1, 2, 3]"s);
    WHEN("using get_as with list") {
      THEN("conversion succeeds") {
        auto maybe_res = get_as<list>(x);
        if (CHECK(maybe_res) && CHECK_EQ(maybe_res->size(), 3u)) {
          auto& res = *maybe_res;
          CHECK_EQ(get_as<int>(res[0]), 1);
          CHECK_EQ(get_as<int>(res[1]), 2);
          CHECK_EQ(get_as<int>(res[2]), 3);
        }
      }
    }
    WHEN("using get_as with vector<int>") {
      THEN("conversion succeeds") {
        auto maybe_res = get_as<std::vector<int>>(x);
        if (CHECK(maybe_res) && CHECK_EQ(maybe_res->size(), 3u)) {
          auto& res = *maybe_res;
          CHECK_EQ(res[0], 1);
          CHECK_EQ(res[1], 2);
          CHECK_EQ(res[2], 3);
        }
      }
    }
  }
}

SCENARIO("get_as can convert config values to dictionaries") {
  using dictionary = config_value::dictionary;
  auto dict = config_value::dictionary{
    {"a", config_value{1}},
    {"b", config_value{2}},
    {"c", config_value{3}},
  };
  std::vector<config_value> given_values;
  given_values.emplace_back(std::move(dict));
  given_values.emplace_back("{a = 1, b = 2, c = 3}"s);
  for (auto& x : given_values) {
    GIVEN("the config value " << x) {
      WHEN("using get_as with config_value::dictionary") {
        THEN("conversion succeeds") {
          auto maybe_res = get_as<dictionary>(x);
          if (CHECK(maybe_res) && CHECK_EQ(maybe_res->size(), 3u)) {
            auto& res = *maybe_res;
            CHECK_EQ(get_as<int>(res["a"]), 1);
            CHECK_EQ(get_as<int>(res["b"]), 2);
            CHECK_EQ(get_as<int>(res["c"]), 3);
          }
        }
      }
      WHEN("using get_as with config_value::list") {
        THEN("CAF converts the dictionary to a list of lists") {
          auto maybe_res = get_as<list>(x);
          if (CHECK(maybe_res) && CHECK_EQ(maybe_res->size(), 3u)) {
            auto& res = *maybe_res;
            if (auto kvp = unbox(get_as<list>(res[0]));
                CHECK_EQ(kvp.size(), 2u)) {
              CHECK_EQ(get_as<std::string>(kvp[0]), "a");
              CHECK_EQ(get_as<int>(kvp[1]), 1);
            }
            if (auto kvp = unbox(get_as<list>(res[1]));
                CHECK_EQ(kvp.size(), 2u)) {
              CHECK_EQ(get_as<std::string>(kvp[0]), "b");
              CHECK_EQ(get_as<int>(kvp[1]), 2);
            }
            if (auto kvp = unbox(get_as<list>(res[2]));
                CHECK_EQ(kvp.size(), 2u)) {
              CHECK_EQ(get_as<std::string>(kvp[0]), "c");
              CHECK_EQ(get_as<int>(kvp[1]), 3);
            }
          }
        }
      }
      WHEN("using get_as with vector<tuple<string, int>>") {
        THEN("CAF converts the dictionary to a list of tuples") {
          using kvp_t = std::tuple<std::string, int>;
          auto maybe_res = get_as<std::vector<kvp_t>>(x);
          MESSAGE("maybe_res: " << maybe_res);
          if (CHECK(maybe_res) && CHECK_EQ(maybe_res->size(), 3u)) {
            auto& res = *maybe_res;
            CHECK_EQ(res[0], kvp_t("a", 1));
            CHECK_EQ(res[1], kvp_t("b", 2));
            CHECK_EQ(res[2], kvp_t("c", 3));
          }
        }
      }
    }
  }
}

SCENARIO("get_as can convert config values to maps") {
  auto dict = config_value::dictionary{
    {"1", config_value{1}},
    {"2", config_value{4}},
    {"3", config_value{9}},
  };
  std::vector<config_value> given_values;
  given_values.emplace_back(std::move(dict));
  given_values.emplace_back("{1 = 1, 2 = 4, 3 = 9}"s);
  for (auto& x : given_values) {
    GIVEN("the config value " << x) {
      WHEN("using get_as with map<string, int>") {
        THEN("conversion succeeds") {
          auto maybe_res = get_as<std::map<std::string, int>>(x);
          if (CHECK(maybe_res) && CHECK_EQ(maybe_res->size(), 3u)) {
            auto& res = *maybe_res;
            CHECK_EQ(res["1"], 1);
            CHECK_EQ(res["2"], 4);
            CHECK_EQ(res["3"], 9);
          }
        }
      }
      WHEN("using get_as with unordered_map<string, int>") {
        THEN("conversion succeeds") {
          auto maybe_res = get_as<std::unordered_map<std::string, int>>(x);
          if (CHECK(maybe_res) && CHECK_EQ(maybe_res->size(), 3u)) {
            auto& res = *maybe_res;
            CHECK_EQ(res["1"], 1);
            CHECK_EQ(res["2"], 4);
            CHECK_EQ(res["3"], 9);
          }
        }
      }
      WHEN("using get_as with map<int, int>") {
        THEN("conversion succeeds") {
          auto maybe_res = get_as<std::map<int, int>>(x);
          if (CHECK(maybe_res) && CHECK_EQ(maybe_res->size(), 3u)) {
            auto& res = *maybe_res;
            CHECK_EQ(res[1], 1);
            CHECK_EQ(res[2], 4);
            CHECK_EQ(res[3], 9);
          }
        }
      }
      WHEN("using get_as with unordered_map<int, int>") {
        THEN("conversion succeeds") {
          auto maybe_res = get_as<std::unordered_map<int, int>>(x);
          if (CHECK(maybe_res) && CHECK_EQ(maybe_res->size(), 3u)) {
            auto& res = *maybe_res;
            CHECK_EQ(res[1], 1);
            CHECK_EQ(res[2], 4);
            CHECK_EQ(res[3], 9);
          }
        }
      }
    }
  }
}

SCENARIO("get_as can convert config values to custom types") {
  std::vector<std::pair<weekday, std::string>> weekday_values{
    {weekday::monday, "monday"s},       {weekday::tuesday, "tuesday"s},
    {weekday::wednesday, "wednesday"s}, {weekday::thursday, "thursday"s},
    {weekday::friday, "friday"s},       {weekday::saturday, "saturday"s},
    {weekday::sunday, "sunday"s}};
  for (const auto& [enum_val, str_val] : weekday_values) {
    config_value x{str_val};
    GIVEN("the config value " << x) {
      WHEN("using get_as with weekday") {
        THEN("CAF picks up the custom inspect_value overload for conversion") {
          auto maybe_res = get_as<weekday>(x);
          if (CHECK(maybe_res))
            CHECK_EQ(*maybe_res, enum_val);
        }
      }
    }
  }
  config_value::dictionary my_request_dict;
  my_request_dict["a"] = config_value{10};
  my_request_dict["b"] = config_value{20};
  auto my_request_val = config_value{my_request_dict};
  GIVEN("the config value " << my_request_val) {
    WHEN("using get_as with my_request") {
      THEN("CAF picks up the custom inspect overload for conversion") {
        auto maybe_res = get_as<my_request>(my_request_val);
        if (CHECK(maybe_res))
          CHECK_EQ(*maybe_res, my_request(10, 20));
      }
    }
  }
  std::vector<config_value> obj_vals{config_value{my_request_val},
                                     config_value{config_value::dictionary{}},
                                     config_value{"{}"s}};
  for (auto& x : obj_vals) {
    GIVEN("the config value " << x) {
      WHEN("using get_as with dummy_tag_type") {
        THEN("CAF only checks whether the config value is dictionary-ish") {
          CHECK(get_as<dummy_tag_type>(my_request_val));
        }
      }
    }
  }
  std::vector<config_value> non_obj_vals{config_value{}, config_value{42},
                                         config_value{"[1,2,3]"s}};
  for (auto& x : non_obj_vals) {
    GIVEN("the config value " << x) {
      WHEN("using get_as with dummy_tag_type") {
        THEN("conversion fails") {
          CHECK_EQ(get_as<dummy_tag_type>(x), sec::conversion_failed);
        }
      }
    }
  }
}

SCENARIO("get_or converts or returns a fallback value") {
  using namespace caf::literals;
  GIVEN("the config value 42") {
    config_value x{42};
    WHEN("using get_or with type int") {
      THEN("CAF ignores the default value") {
        CHECK_EQ(get_or(x, 10), 42);
      }
    }
    WHEN("using get_or with type string") {
      THEN("CAF ignores the default value") {
        CHECK_EQ(get_or(x, "foo"s), "42"s);
      }
    }
    WHEN("using get_or with type bool") {
      THEN("CAF returns the default value") {
        CHECK_EQ(get_or(x, false), false);
      }
    }
    WHEN("using get_or with type span<int>") {
      int fallback_arr[] = {10, 20, 30};
      auto fallback = make_span(fallback_arr);
      THEN("CAF returns the default value after converting it to vector<int>") {
        auto result = get_or(x, fallback);
        static_assert(std::is_same<decltype(result), std::vector<int>>::value);
        CHECK_EQ(result, std::vector<int>({10, 20, 30}));
      }
    }
  }
}

SCENARIO("config values can default-construct all registered types") {
  auto from = [](type_id_t id) {
    config_value result;
    if (auto err = result.default_construct(id))
      CAF_FAIL("default construction failed: " << err);
    return result;
  };
  auto keys = [](const auto& dict) {
    std::vector<std::string> result;
    for (const auto& kvp : dict)
      result.emplace_back(kvp.first);
    return result;
  };
  GIVEN("a config value") {
    WHEN("calling default_construct for any integral type") {
      THEN("the config value becomes config_value::integer{0}") {
        CHECK_EQ(from(type_id_v<int8_t>), config_value{0});
        CHECK_EQ(from(type_id_v<int16_t>), config_value{0});
        CHECK_EQ(from(type_id_v<int32_t>), config_value{0});
        CHECK_EQ(from(type_id_v<int64_t>), config_value{0});
        CHECK_EQ(from(type_id_v<uint8_t>), config_value{0});
        CHECK_EQ(from(type_id_v<uint16_t>), config_value{0});
        CHECK_EQ(from(type_id_v<uint32_t>), config_value{0});
        CHECK_EQ(from(type_id_v<uint64_t>), config_value{0});
      }
    }
    WHEN("calling default_construct for any floating point type") {
      THEN("the config value becomes config_value::real{0}") {
        CHECK_EQ(from(type_id_v<float>), config_value{0.0});
        CHECK_EQ(from(type_id_v<double>), config_value{0.0});
        CHECK_EQ(from(type_id_v<long double>), config_value{0.0});
      }
    }
    WHEN("calling default_construct for std::string") {
      THEN("the config value becomes \"\"") {
        CHECK_EQ(from(type_id_v<std::string>), config_value{std::string{}});
      }
    }
    WHEN("calling default_construct for caf::timespan") {
      THEN("the config value becomes 0s") {
        CHECK_EQ(from(type_id_v<timespan>), config_value{timespan{0}});
      }
    }
    WHEN("calling default_construct for caf::uri") {
      THEN("the config value becomes an empty URI") {
        CHECK_EQ(from(type_id_v<uri>), config_value{uri{}});
      }
    }
    WHEN("calling default_construct for any list-like type") {
      THEN("the config value becomes a config_value::list") {
        CHECK_EQ(from(type_id_v<std::vector<actor>>).get_data().index(), 7u);
        CHECK_EQ(from(type_id_v<std::vector<bool>>).get_data().index(), 7u);
      }
    }
    WHEN("calling default_construct for any custom non-list type") {
      THEN("the config value becomes a dictionary") {
        auto val = from(type_id_v<my_request>);
        CHECK_EQ(val.get_data().index(), 8u);
        auto& dict = val.as_dictionary();
        CHECK_EQ(keys(dict), std::vector<std::string>({"@type", "a", "b"}));
        CHECK_EQ(dict["@type"].get_data().index(), 6u);
        CHECK_EQ(get_as<std::string>(dict["@type"]), "my_request"s);
        CHECK_EQ(dict["a"].get_data().index(), 1u);
        CHECK_EQ(get_as<int32_t>(dict["a"]), 0);
        CHECK_EQ(dict["b"].get_data().index(), 1u);
        CHECK_EQ(get_as<int32_t>(dict["b"]), 0);
      }
    }
  }
}

#define CHECK_ROUNDTRIP(init_val, expected_str)                                \
  do {                                                                         \
    config_value x;                                                            \
    if (auto assign_failed = x.assign(init_val); CHECK(!assign_failed)) {      \
      auto str = to_string(x);                                                 \
      CHECK_EQ(str, expected_str);                                             \
      auto parsed = config_value::parse(str);                                  \
      using init_val_type = decltype(init_val);                                \
      if (CHECK(parsed)) {                                                     \
        if constexpr (!std::is_same<init_val_type, message>::value)            \
          CHECK_EQ(get_as<init_val_type>(*parsed), init_val);                  \
        else                                                                   \
          CHECK_EQ(to_string(*parsed), str);                                   \
      }                                                                        \
    }                                                                          \
  } while (false)

SCENARIO("config values can parse their own to_string output") {
  GIVEN("a config value") {
    WHEN("assigning a value and then calling to_string on it") {
      THEN("then config_value::parse reconstitutes the original value") {
        CHECK_ROUNDTRIP(0, "0");
        CHECK_ROUNDTRIP("hello world"s, "hello world");
        CHECK_ROUNDTRIP(std::vector<int>({1, 2, 3}), "[1, 2, 3]");
        CHECK_ROUNDTRIP(my_request(1, 2),
                        R"_({"@type" = "my_request", a = 1, b = 2})_");
        CHECK_ROUNDTRIP(std::make_tuple(add_atom_v, 1, 2),
                        R"_([{"@type" = "caf::add_atom"}, 1, 2])_");
        CHECK_ROUNDTRIP(make_message(add_atom_v, 1, 2),
                        R"_([{"@type" = "caf::add_atom"}, 1, 2])_");
      }
    }
  }
}

CAF_TEST(default_constructed) {
  config_value x;
  CAF_CHECK_EQUAL(holds_alternative<none_t>(x), true);
  CAF_CHECK_EQUAL(x.type_name(), "none"s);
}

CAF_TEST(positive integer) {
  config_value x{4200};
  CAF_CHECK_EQUAL(holds_alternative<int64_t>(x), true);
  CAF_CHECK_EQUAL(get<int64_t>(x), 4200);
  CAF_CHECK(get_if<int64_t>(&x) != nullptr);
  CAF_CHECK_EQUAL(holds_alternative<uint64_t>(x), true);
  CAF_CHECK_EQUAL(get<uint64_t>(x), 4200u);
  CAF_CHECK_EQUAL(get_if<uint64_t>(&x), uint64_t{4200});
  CAF_CHECK_EQUAL(holds_alternative<int>(x), true);
  CAF_CHECK_EQUAL(get<int>(x), 4200);
  CAF_CHECK_EQUAL(get_if<int>(&x), 4200);
  CAF_CHECK_EQUAL(holds_alternative<int16_t>(x), true);
  CAF_CHECK_EQUAL(get<int16_t>(x), 4200);
  CAF_CHECK_EQUAL(get_if<int16_t>(&x), int16_t{4200});
  CAF_CHECK_EQUAL(holds_alternative<int8_t>(x), false);
  CAF_CHECK_EQUAL(get_if<int8_t>(&x), caf::none);
}

CAF_TEST(negative integer) {
  config_value x{-1};
  CAF_CHECK_EQUAL(holds_alternative<int64_t>(x), true);
  CAF_CHECK_EQUAL(get<int64_t>(x), -1);
  CAF_CHECK(get_if<int64_t>(&x) != nullptr);
  CAF_CHECK_EQUAL(holds_alternative<uint64_t>(x), false);
  CAF_CHECK_EQUAL(get_if<uint64_t>(&x), none);
  CAF_CHECK_EQUAL(holds_alternative<int>(x), true);
  CAF_CHECK_EQUAL(get<int>(x), -1);
  CAF_CHECK_EQUAL(get_if<int>(&x), -1);
  CAF_CHECK_EQUAL(holds_alternative<int16_t>(x), true);
  CAF_CHECK_EQUAL(get<int16_t>(x), -1);
  CAF_CHECK_EQUAL(get_if<int16_t>(&x), int16_t{-1});
  CAF_CHECK_EQUAL(holds_alternative<int8_t>(x), true);
  CAF_CHECK_EQUAL(get_if<int8_t>(&x), int8_t{-1});
  CAF_CHECK_EQUAL(holds_alternative<uint8_t>(x), false);
  CAF_CHECK_EQUAL(get_if<uint8_t>(&x), none);
}

CAF_TEST(timespan) {
  timespan ns500{500};
  config_value x{ns500};
  CAF_CHECK_EQUAL(holds_alternative<timespan>(x), true);
  CAF_CHECK_EQUAL(get<timespan>(x), ns500);
  CAF_CHECK_NOT_EQUAL(get_if<timespan>(&x), nullptr);
}

CAF_TEST(homogeneous list) {
  using integer_list = std::vector<int64_t>;
  auto xs = make_config_value_list(1, 2, 3);
  auto ys = config_value{integer_list{1, 2, 3}};
  CAF_CHECK_EQUAL(xs, ys);
  CAF_CHECK_EQUAL(to_string(xs), "[1, 2, 3]");
  CAF_CHECK_EQUAL(xs.type_name(), "list"s);
  CAF_CHECK_EQUAL(holds_alternative<config_value::list>(xs), true);
  CAF_CHECK_EQUAL(holds_alternative<integer_list>(xs), true);
  CAF_CHECK_EQUAL(get<integer_list>(xs), integer_list({1, 2, 3}));
}

CAF_TEST(heterogeneous list) {
  auto xs_value = make_config_value_list(1, "two", 3.0);
  auto& xs = xs_value.as_list();
  CAF_CHECK_EQUAL(xs_value.type_name(), "list"s);
  CAF_REQUIRE_EQUAL(xs.size(), 3u);
  CAF_CHECK_EQUAL(xs[0], 1);
  CAF_CHECK_EQUAL(xs[1], "two"s);
  CAF_CHECK_EQUAL(xs[2], 3.0);
}

CAF_TEST(convert_to_list) {
  config_value x{int64_t{42}};
  CAF_CHECK_EQUAL(x.type_name(), "integer"s);
  CAF_CHECK_EQUAL(to_string(x), "42");
  x.convert_to_list();
  CAF_CHECK_EQUAL(x.type_name(), "list"s);
  CAF_CHECK_EQUAL(to_string(x), "[42]");
  x.convert_to_list();
  CAF_CHECK_EQUAL(to_string(x), "[42]");
}

CAF_TEST(append) {
  config_value x{int64_t{1}};
  CAF_CHECK_EQUAL(to_string(x), "1");
  x.append(config_value{int64_t{2}});
  CAF_CHECK_EQUAL(to_string(x), "[1, 2]");
  x.append(config_value{"foo"});
  CAF_CHECK_EQUAL(to_string(x), R"__([1, 2, "foo"])__");
}

CAF_TEST(homogeneous dictionary) {
  using integer_map = caf::dictionary<int64_t>;
  auto xs = dict()
              .add("value-1", config_value{100000})
              .add("value-2", config_value{2})
              .add("value-3", config_value{3})
              .add("value-4", config_value{4})
              .make();
  integer_map ys{
    {"value-1", 100000},
    {"value-2", 2},
    {"value-3", 3},
    {"value-4", 4},
  };
  config_value xs_cv{xs};
  if (auto val = get_if<int64_t>(&xs, "value-1"))
    CAF_CHECK_EQUAL(*val, int64_t{100000});
  else
    CAF_FAIL("value-1 not an int64_t");
  CAF_CHECK_EQUAL(get_if<int32_t>(&xs, "value-1"), int32_t{100000});
  CAF_CHECK_EQUAL(get_if<int16_t>(&xs, "value-1"), none);
  CAF_CHECK_EQUAL(get<int64_t>(xs, "value-1"), 100000);
  CAF_CHECK_EQUAL(get<int32_t>(xs, "value-1"), 100000);
  CAF_CHECK_EQUAL(get_if<integer_map>(&xs_cv), ys);
  CAF_CHECK_EQUAL(get<integer_map>(xs_cv), ys);
}

CAF_TEST(heterogeneous dictionary) {
  using string_list = std::vector<string>;
  auto xs = dict()
              .add("scheduler", dict()
                                  .add("policy", config_value{"none"})
                                  .add("max-threads", config_value{2})
                                  .make_cv())
              .add("nodes", dict()
                              .add("preload", cfg_lst("sun", "venus", "mercury",
                                                      "earth", "mars"))
                              .make_cv())

              .make();
  CAF_CHECK_EQUAL(get<string>(xs, "scheduler.policy"), "none");
  CAF_CHECK_EQUAL(get<int64_t>(xs, "scheduler.max-threads"), 2);
  CAF_CHECK_EQUAL(get_if<double>(&xs, "scheduler.max-threads"), nullptr);
  string_list nodes{"sun", "venus", "mercury", "earth", "mars"};
  CAF_CHECK_EQUAL(get<string_list>(xs, "nodes.preload"), nodes);
}

CAF_TEST(successful parsing) {
  // Store the parsed value on the stack, because the unit test framework takes
  // references when comparing values. Since we call get<T>() on the result of
  // parse(), we would end up with a reference to a temporary.
  config_value parsed;
  auto parse = [&](const string& str) -> config_value& {
    auto x = config_value::parse(str);
    if (!x)
      CAF_FAIL("cannot parse " << str << ": assumed a result but error "
                               << to_string(x.error()));
    parsed = std::move(*x);
    return parsed;
  };
  using di = caf::dictionary<int>; // Dictionary-of-integers.
  using ls = std::vector<string>;  // List-of-strings.
  using li = std::vector<int>;     // List-of-integers.
  using lli = std::vector<li>;     // List-of-list-of-integers.
  using std::chrono::milliseconds;
  CAF_CHECK_EQUAL(get<int64_t>(parse("123")), 123);
  CAF_CHECK_EQUAL(get<int64_t>(parse("+123")), 123);
  CAF_CHECK_EQUAL(get<int64_t>(parse("-1")), -1);
  CAF_CHECK_EQUAL(get<double>(parse("1.")), 1.);
  CAF_CHECK_EQUAL(get<string>(parse("\"abc\"")), "abc");
  CAF_CHECK_EQUAL(get<string>(parse("abc")), "abc");
  CAF_CHECK_EQUAL(get<li>(parse("[1, 2, 3]")), li({1, 2, 3}));
  CAF_CHECK_EQUAL(get<ls>(parse("[\"abc\", \"def\", \"ghi\"]")),
                  ls({"abc", "def", "ghi"}));
  CAF_CHECK_EQUAL(get<lli>(parse("[[1, 2], [3]]")), lli({li{1, 2}, li{3}}));
  CAF_CHECK_EQUAL(get<timespan>(parse("10ms")), milliseconds(10));
  CAF_CHECK_EQUAL(get<di>(parse("{a=1,b=2}")), di({{"a", 1}, {"b", 2}}));
}

#define CHECK_CLI_PARSE(type, str, ...)                                        \
  do {                                                                         \
    /* Note: parse_impl from make_config_option.hpp internally dispatches   */ \
    /*       to parse_cli. No need to replicate that wrapping code here.    */ \
    if (auto res = caf::detail::parse_impl<type>(nullptr, str)) {              \
      type expected_res{__VA_ARGS__};                                          \
      if (auto unboxed = ::caf::get_if<type>(std::addressof(*res))) {          \
        if (*unboxed == expected_res)                                          \
          CAF_CHECK_PASSED("parse(" << str << ") == " << expected_res);        \
        else                                                                   \
          CAF_CHECK_FAILED(*unboxed << " != " << expected_res);                \
      } else {                                                                 \
        CAF_CHECK_FAILED(*res << " != " << expected_res);                      \
      }                                                                        \
    } else {                                                                   \
      CAF_CHECK_FAILED("parse(" << str << ") -> " << res.error());             \
    }                                                                          \
  } while (false)

#define CHECK_CLI_PARSE_FAILS(type, str)                                       \
  do {                                                                         \
    if (auto res = caf::detail::parse_impl<type>(nullptr, str)) {              \
      CAF_CHECK_FAILED("unexpected parser result: " << *res);                  \
    } else {                                                                   \
      CAF_CHECK_PASSED("parse(" << str << ") == " << res.error());             \
    }                                                                          \
  } while (false)

CAF_TEST(parsing via parse_cli enables shortcut syntax for some types) {
  using ls = std::vector<string>; // List-of-strings.
  using li = std::vector<int>;    // List-of-integers.
  using lli = std::vector<li>;    // List-of-list-of-integers.
  CAF_MESSAGE("lists can omit square brackets");
  CHECK_CLI_PARSE(int, "123", 123);
  CHECK_CLI_PARSE(li, "[ 1,2 , 3  ,]", 1, 2, 3);
  CHECK_CLI_PARSE(li, "[ 1,2 , 3  ]", 1, 2, 3);
  CHECK_CLI_PARSE(li, " 1,2 , 3  ,", 1, 2, 3);
  CHECK_CLI_PARSE(li, " 1,2 , 3  ", 1, 2, 3);
  CHECK_CLI_PARSE(li, " [  ] ", li{});
  CHECK_CLI_PARSE(li, "  ", li{});
  CHECK_CLI_PARSE(li, "", li{});
  CHECK_CLI_PARSE(li, "[123]", 123);
  CHECK_CLI_PARSE(li, "123", 123);
  CAF_MESSAGE("brackets must have matching opening/closing brackets");
  CHECK_CLI_PARSE_FAILS(li, " 1,2 , 3  ,]");
  CHECK_CLI_PARSE_FAILS(li, " 1,2 , 3  ]");
  CHECK_CLI_PARSE_FAILS(li, "123]");
  CHECK_CLI_PARSE_FAILS(li, "[ 1,2 , 3  ,");
  CHECK_CLI_PARSE_FAILS(li, "[ 1,2 , 3  ");
  CHECK_CLI_PARSE_FAILS(li, "[123");
  CAF_MESSAGE("string lists can omit quotation marks");
  CHECK_CLI_PARSE(string, R"_("123")_", "123");
  CHECK_CLI_PARSE(string, R"_(123)_", "123");
  CHECK_CLI_PARSE(ls, R"_([ "1 ","2" , "3"  ,])_", "1 ", "2", "3");
  CHECK_CLI_PARSE(ls, R"_([ 1,2 , 3  ,])_", "1", "2", "3");
  CHECK_CLI_PARSE(ls, R"_([ 1,2 , 3  ])_", "1", "2", "3");
  CHECK_CLI_PARSE(ls, R"_( 1,2 , 3  ,)_", "1", "2", "3");
  CHECK_CLI_PARSE(ls, R"_( 1,2 , 3  )_", "1", "2", "3");
  CHECK_CLI_PARSE(ls, R"_( [  ] )_", ls{});
  CHECK_CLI_PARSE(ls, R"_(  )_", ls{});
  CHECK_CLI_PARSE(ls, R"_(["abc"])_", "abc");
  CHECK_CLI_PARSE(ls, R"_([abc])_", "abc");
  CHECK_CLI_PARSE(ls, R"_("abc")_", "abc");
  CHECK_CLI_PARSE(ls, R"_(abc)_", "abc");
  CAF_MESSAGE("nested lists can omit the outer square brackets");
  CHECK_CLI_PARSE(lli, "[[1, 2, 3, ], ]", li({1, 2, 3}));
  CHECK_CLI_PARSE(lli, "[[1, 2, 3]]", li({1, 2, 3}));
  CHECK_CLI_PARSE(lli, "[1, 2, 3, ]", li({1, 2, 3}));
  CHECK_CLI_PARSE(lli, "[1, 2, 3]", li({1, 2, 3}));
  CHECK_CLI_PARSE(lli, "[[1], [2]]", li({1}), li({2}));
  CHECK_CLI_PARSE(lli, "[1], [2]", li({1}), li({2}));
  CHECK_CLI_PARSE_FAILS(lli, "1");
  CHECK_CLI_PARSE_FAILS(lli, "1, 2");
  CHECK_CLI_PARSE_FAILS(lli, "[1, 2]]");
  CHECK_CLI_PARSE_FAILS(lli, "[[1, 2]");
}

CAF_TEST(unsuccessful parsing) {
  auto parse = [](const string& str) {
    auto x = config_value::parse(str);
    if (x)
      CAF_FAIL("assumed an error but got a result");
    return std::move(x.error());
  };
  CAF_CHECK_EQUAL(parse("10msb"), pec::trailing_character);
  CAF_CHECK_EQUAL(parse("10foo"), pec::trailing_character);
  CAF_CHECK_EQUAL(parse("[1,"), pec::unexpected_eof);
  CAF_CHECK_EQUAL(parse("{a=,"), pec::unexpected_character);
  CAF_CHECK_EQUAL(parse("{a=1,"), pec::unexpected_eof);
  CAF_CHECK_EQUAL(parse("{a=1 b=2}"), pec::unexpected_character);
}

CAF_TEST(conversion to simple tuple) {
  using tuple_type = std::tuple<size_t, std::string>;
  config_value x{42};
  x.as_list().emplace_back("hello world");
  CAF_REQUIRE(holds_alternative<tuple_type>(x));
  CAF_REQUIRE_NOT_EQUAL(get_if<tuple_type>(&x), none);
  CAF_CHECK_EQUAL(get<tuple_type>(x),
                  std::make_tuple(size_t{42}, "hello world"s));
}

CAF_TEST(conversion to nested tuple) {
  using inner_tuple_type = std::tuple<int, int>;
  using tuple_type = std::tuple<size_t, inner_tuple_type>;
  config_value x{42};
  x.as_list().emplace_back(make_config_value_list(2, 40));
  CAF_REQUIRE(holds_alternative<tuple_type>(x));
  CAF_REQUIRE_NOT_EQUAL(get_if<tuple_type>(&x), none);
  CAF_CHECK_EQUAL(get<tuple_type>(x),
                  std::make_tuple(size_t{42}, std::make_tuple(2, 40)));
}

CAF_TEST(conversion to std::vector) {
  using list_type = std::vector<int>;
  auto xs = make_config_value_list(1, 2, 3, 4);
  CAF_CHECK(holds_alternative<list_type>(xs));
  auto ys = get_if<list_type>(&xs);
  CAF_REQUIRE(ys);
  CAF_CHECK_EQUAL(*ys, list_type({1, 2, 3, 4}));
}

CAF_TEST(conversion to std::list) {
  using list_type = std::list<int>;
  auto xs = make_config_value_list(1, 2, 3, 4);
  CAF_CHECK(holds_alternative<list_type>(xs));
  auto ys = get_if<list_type>(&xs);
  CAF_REQUIRE(ys);
  CAF_CHECK_EQUAL(*ys, list_type({1, 2, 3, 4}));
}

CAF_TEST(conversion to std::set) {
  using list_type = std::set<int>;
  auto xs = make_config_value_list(1, 2, 3, 4);
  CAF_CHECK(holds_alternative<list_type>(xs));
  auto ys = get_if<list_type>(&xs);
  CAF_REQUIRE(ys);
  CAF_CHECK_EQUAL(*ys, list_type({1, 2, 3, 4}));
}

CAF_TEST(conversion to std::unordered_set) {
  using list_type = std::unordered_set<int>;
  auto xs = make_config_value_list(1, 2, 3, 4);
  CAF_CHECK(holds_alternative<list_type>(xs));
  auto ys = get_if<list_type>(&xs);
  CAF_REQUIRE(ys);
  CAF_CHECK_EQUAL(*ys, list_type({1, 2, 3, 4}));
}

CAF_TEST(conversion to std::map) {
  using map_type = std::map<std::string, int>;
  auto xs = dict().add("a", 1).add("b", 2).add("c", 3).add("d", 4).make_cv();
  CAF_CHECK(holds_alternative<map_type>(xs));
  auto ys = get_if<map_type>(&xs);
  CAF_REQUIRE(ys);
  CAF_CHECK_EQUAL(*ys, map_type({{"a", 1}, {"b", 2}, {"c", 3}, {"d", 4}}));
}

CAF_TEST(conversion to std::multimap) {
  using map_type = std::multimap<std::string, int>;
  auto xs = dict().add("a", 1).add("b", 2).add("c", 3).add("d", 4).make_cv();
  CAF_CHECK(holds_alternative<map_type>(xs));
  auto ys = get_if<map_type>(&xs);
  CAF_REQUIRE(ys);
  CAF_CHECK_EQUAL(*ys, map_type({{"a", 1}, {"b", 2}, {"c", 3}, {"d", 4}}));
}

CAF_TEST(conversion to std::unordered_map) {
  using map_type = std::unordered_map<std::string, int>;
  auto xs = dict().add("a", 1).add("b", 2).add("c", 3).add("d", 4).make_cv();
  CAF_CHECK(holds_alternative<map_type>(xs));
  auto ys = get_if<map_type>(&xs);
  CAF_REQUIRE(ys);
  CAF_CHECK_EQUAL(*ys, map_type({{"a", 1}, {"b", 2}, {"c", 3}, {"d", 4}}));
}

CAF_TEST(conversion to std::unordered_multimap) {
  using map_type = std::unordered_multimap<std::string, int>;
  auto xs = dict().add("a", 1).add("b", 2).add("c", 3).add("d", 4).make_cv();
  CAF_CHECK(holds_alternative<map_type>(xs));
  auto ys = get_if<map_type>(&xs);
  CAF_REQUIRE(ys);
  CAF_CHECK_EQUAL(*ys, map_type({{"a", 1}, {"b", 2}, {"c", 3}, {"d", 4}}));
}

CAF_TEST_FIXTURE_SCOPE_END()
