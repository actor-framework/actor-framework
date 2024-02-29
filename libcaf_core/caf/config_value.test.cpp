// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/config_value.hpp"

#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/deep_to_string.hpp"
#include "caf/detail/bounds_checker.hpp"
#include "caf/detail/overload.hpp"
#include "caf/detail/parse.hpp"
#include "caf/init_global_meta_objects.hpp"
#include "caf/log/test.hpp"
#include "caf/make_config_option.hpp"
#include "caf/none.hpp"
#include "caf/pec.hpp"
#include "caf/typed_actor.hpp"

#include <cmath>
#include <list>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using std::string;

using namespace caf;

using namespace std::string_literals;

namespace {

struct my_request;
struct dummy_tag_type;

} // namespace

CAF_BEGIN_TYPE_ID_BLOCK(config_value_test, caf::first_custom_type_id + 75)

  CAF_ADD_TYPE_ID(config_value_test, (my_request))
  CAF_ADD_TYPE_ID(config_value_test, (dummy_tag_type))
  CAF_ADD_TYPE_ID(config_value_test, (std::vector<bool>) )
  CAF_ADD_TYPE_ID(config_value_test, (std::map<int32_t, int32_t>) )
  CAF_ADD_TYPE_ID(config_value_test, (std::map<std::string, std::u16string>) )
  CAF_ADD_TYPE_ID(config_value_test, (std::tuple<int32_t, int32_t, int32_t>) )
  CAF_ADD_TYPE_ID(config_value_test,
                  (std::tuple<std::string, int32_t, uint32_t>) )
  CAF_ADD_TYPE_ID(config_value_test, (std::vector<int32_t>) )
  CAF_ADD_TYPE_ID(config_value_test, (std::vector<std::string>) )

CAF_END_TYPE_ID_BLOCK(config_value_test)

namespace {

using list = config_value::list;

using dictionary = config_value::dictionary;

struct my_request {
  int32_t a = 0;
  int32_t b = 0;
  my_request() = default;
  my_request(int a, int b) : a(a), b(b) {
    // nop
  }
};

inline bool operator==(const my_request& x, const my_request& y) {
  return std::tie(x.a, x.b) == std::tie(y.a, y.b);
}

template <class Inspector>
bool inspect(Inspector& f, my_request& x) {
  return f.object(x).fields(f.field("a", x.a), f.field("b", x.b));
}

struct dummy_tag_type {};

constexpr bool operator==(dummy_tag_type, dummy_tag_type) {
  return true;
}

template <class T>
T unbox(caf::expected<T> x) {
  if (!x)
    test::runnable::current().fail("{}", to_string(x.error()));
  return std::move(*x);
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

WITH_FIXTURE(fixture) {

SCENARIO("default-constructed config values represent null") {
  GIVEN("a default-constructed config value") {
    config_value x;
    WHEN("inspecting the config value") {
      THEN("its type is none and its to_string output is 'null'") {
        check(holds_alternative<none_t>(x));
        check_eq(x.type_name(), "none"s);
        check_eq(to_string(x), "null"s);
      }
    }
  }
}

SCENARIO("get_as can convert config values to boolean") {
  GIVEN("a config value x with value true or false") {
    WHEN("using get_as with bool") {
      THEN("conversion succeeds") {
        check_eq(get_as<bool>(cv_true), true);
        check_eq(get_as<bool>(cv_false), false);
      }
    }
  }
  GIVEN("a config value x with value \"true\" or \"false\"") {
    WHEN("using get_as with bool") {
      THEN("conversion succeeds") {
        check_eq(get_as<bool>(config_value{"true"s}), true);
        check_eq(get_as<bool>(config_value{"false"s}), false);
      }
    }
  }
  GIVEN("a config value with type annotation 'bool' and the value \"true\"") {
    config_value x;
    x.as_dictionary().emplace("@type", "bool");
    x.as_dictionary().emplace("value", "true");
    WHEN("using get_as with bool") {
      THEN("conversion succeeds") {
        check_eq(get_as<bool>(x), true);
      }
    }
  }
  GIVEN("non-boolean config_values") {
    WHEN("using get_as with bool") {
      THEN("conversion fails") {
        check_eq(get_as<bool>(cv_null), sec::conversion_failed);
        check_eq(get_as<bool>(cv_empty_uri), sec::conversion_failed);
        check_eq(get_as<bool>(cv_empty_list), sec::conversion_failed);
        check_eq(get_as<bool>(cv_empty_dict), sec::conversion_failed);
        check_eq(get_as<bool>(config_value{0}), sec::conversion_failed);
        check_eq(get_as<bool>(config_value{1}), sec::conversion_failed);
        check_eq(get_as<bool>(config_value{0.f}), sec::conversion_failed);
        check_eq(get_as<bool>(config_value{1.f}), sec::conversion_failed);
        check_eq(get_as<bool>(config_value{""s}), sec::conversion_failed);
        check_eq(get_as<bool>(config_value{"1"s}), sec::conversion_failed);
      }
    }
  }
}

SCENARIO("get_as can convert config values to integers") {
  GIVEN("a config value x with value 32,768") {
    auto x = config_value{32'768};
    WHEN("using get_as with integer types") {
      THEN("conversion fails if bounds checks fail") {
        check_eq(get_as<uint64_t>(x), 32'768u);
        check_eq(get_as<int64_t>(x), 32'768);
        check_eq(get_as<uint32_t>(x), 32'768u);
        check_eq(get_as<int32_t>(x), 32'768);
        check_eq(get_as<uint16_t>(x), 32'768u);
        check_eq(get_as<int16_t>(x), sec::conversion_failed);
        check_eq(get_as<uint8_t>(x), sec::conversion_failed);
        check_eq(get_as<int8_t>(x), sec::conversion_failed);
      }
    }
  }
  GIVEN("a config value x with value -5") {
    auto x = config_value{-5};
    WHEN("using get_as with integer types") {
      THEN("conversion fails for all unsigned types") {
        check_eq(get_as<uint64_t>(x), sec::conversion_failed);
        check_eq(get_as<int64_t>(x), -5);
        check_eq(get_as<uint32_t>(x), sec::conversion_failed);
        check_eq(get_as<int32_t>(x), -5);
        check_eq(get_as<uint16_t>(x), sec::conversion_failed);
        check_eq(get_as<int16_t>(x), -5);
        check_eq(get_as<uint8_t>(x), sec::conversion_failed);
        check_eq(get_as<int8_t>(x), -5);
      }
    }
  }
  GIVEN("a config value x with value \"50000\"") {
    auto x = config_value{"50000"s};
    WHEN("using get_as with integer types") {
      THEN("CAF parses the string and performs a bound check") {
        check_eq(get_as<uint64_t>(x), 50'000u);
        check_eq(get_as<int64_t>(x), 50'000);
        check_eq(get_as<uint32_t>(x), 50'000u);
        check_eq(get_as<int32_t>(x), 50'000);
        check_eq(get_as<uint16_t>(x), 50'000u);
        check_eq(get_as<int16_t>(x), sec::conversion_failed);
        check_eq(get_as<uint8_t>(x), sec::conversion_failed);
        check_eq(get_as<int8_t>(x), sec::conversion_failed);
      }
    }
  }
  GIVEN("a config value x with type annotation 'int32_t' and the value 50") {
    config_value x;
    x.as_dictionary().emplace("@type", "int32_t");
    x.as_dictionary().emplace("value", 50);
    WHEN("using get_as with integer types") {
      THEN("CAF parses the integer and performs a bound check") {
        check_eq(get_as<uint64_t>(x), 50u);
        check_eq(get_as<int64_t>(x), 50);
        check_eq(get_as<uint32_t>(x), 50u);
        check_eq(get_as<int32_t>(x), 50);
        check_eq(get_as<uint16_t>(x), 50u);
        check_eq(get_as<int16_t>(x), 50);
        check_eq(get_as<uint8_t>(x), 50u);
        check_eq(get_as<int8_t>(x), 50);
      }
    }
  }
  GIVEN("a config value x with value 50.0") {
    auto x = config_value{50.0};
    WHEN("using get_as with integer types") {
      THEN("CAF parses the string and performs a bound check") {
        check_eq(get_as<uint64_t>(x), 50u);
        check_eq(get_as<int64_t>(x), 50);
        check_eq(get_as<uint32_t>(x), 50u);
        check_eq(get_as<int32_t>(x), 50);
        check_eq(get_as<uint16_t>(x), 50u);
        check_eq(get_as<int16_t>(x), 50);
        check_eq(get_as<uint8_t>(x), 50u);
        check_eq(get_as<int8_t>(x), 50);
      }
    }
  }
  GIVEN("a config value x with value 50.05") {
    auto x = config_value{50.05};
    WHEN("using get_as with integer types") {
      THEN("CAF fails to convert the real to an integer") {
        check_eq(get_as<uint64_t>(x), sec::conversion_failed);
        check_eq(get_as<int64_t>(x), sec::conversion_failed);
        check_eq(get_as<uint32_t>(x), sec::conversion_failed);
        check_eq(get_as<int32_t>(x), sec::conversion_failed);
        check_eq(get_as<uint16_t>(x), sec::conversion_failed);
        check_eq(get_as<int16_t>(x), sec::conversion_failed);
        check_eq(get_as<uint8_t>(x), sec::conversion_failed);
        check_eq(get_as<int8_t>(x), sec::conversion_failed);
      }
    }
  }
  GIVEN("a config value x with value \"50.000\"") {
    auto x = config_value{"50.000"s};
    WHEN("using get_as with integer types") {
      THEN("CAF parses the string and performs a bound check") {
        check_eq(get_as<uint64_t>(x), 50u);
        check_eq(get_as<int64_t>(x), 50);
        check_eq(get_as<uint32_t>(x), 50u);
        check_eq(get_as<int32_t>(x), 50);
        check_eq(get_as<uint16_t>(x), 50u);
        check_eq(get_as<int16_t>(x), 50);
        check_eq(get_as<uint8_t>(x), 50u);
        check_eq(get_as<int8_t>(x), 50);
      }
    }
  }
  GIVEN("a config value x with value \"50.05\"") {
    auto x = config_value{"50.05"s};
    WHEN("using get_as with integer types") {
      THEN("CAF fails to convert the real to an integer") {
        check_eq(get_as<uint64_t>(x), sec::conversion_failed);
        check_eq(get_as<int64_t>(x), sec::conversion_failed);
        check_eq(get_as<uint32_t>(x), sec::conversion_failed);
        check_eq(get_as<int32_t>(x), sec::conversion_failed);
        check_eq(get_as<uint16_t>(x), sec::conversion_failed);
        check_eq(get_as<int16_t>(x), sec::conversion_failed);
        check_eq(get_as<uint8_t>(x), sec::conversion_failed);
        check_eq(get_as<int8_t>(x), sec::conversion_failed);
      }
    }
  }
  GIVEN("config_values of null, URI, boolean, list or dictionary") {
    WHEN("using get_as with floating point types") {
      THEN("conversion fails") {
        check_eq(get_as<float>(cv_null), sec::conversion_failed);
        check_eq(get_as<float>(cv_true), sec::conversion_failed);
        check_eq(get_as<float>(cv_false), sec::conversion_failed);
        check_eq(get_as<float>(cv_empty_uri), sec::conversion_failed);
        check_eq(get_as<float>(cv_empty_list), sec::conversion_failed);
        check_eq(get_as<float>(cv_empty_dict), sec::conversion_failed);
        check_eq(get_as<double>(cv_null), sec::conversion_failed);
        check_eq(get_as<double>(cv_true), sec::conversion_failed);
        check_eq(get_as<double>(cv_false), sec::conversion_failed);
        check_eq(get_as<double>(cv_empty_uri), sec::conversion_failed);
        check_eq(get_as<double>(cv_empty_list), sec::conversion_failed);
        check_eq(get_as<double>(cv_empty_dict), sec::conversion_failed);
        check_eq(get_as<long double>(cv_null), sec::conversion_failed);
        check_eq(get_as<long double>(cv_true), sec::conversion_failed);
        check_eq(get_as<long double>(cv_false), sec::conversion_failed);
        check_eq(get_as<long double>(cv_empty_uri), sec::conversion_failed);
        check_eq(get_as<long double>(cv_empty_list), sec::conversion_failed);
        check_eq(get_as<long double>(cv_empty_dict), sec::conversion_failed);
      }
    }
  }
}

SCENARIO("get_as can convert config values to floating point numbers") {
  GIVEN("a config value x with value 1.79769e+308") {
    auto x = config_value{1.79769e+308};
    WHEN("using get_as with floating point types") {
      THEN("conversion fails if bounds checks fail") {
        check_eq(get_as<long double>(x), 1.79769e+308);
        check_eq(get_as<double>(x), 1.79769e+308);
        check_eq(get_as<float>(x), sec::conversion_failed);
      }
    }
  }
  GIVEN("a config value x with value \"3e7\"") {
    auto x = config_value{"3e7"s};
    WHEN("using get_as with floating point types") {
      THEN("CAF parses the string and converts the value") {
        check_eq(get_as<long double>(x), 3e7);
        check_eq(get_as<double>(x), 3e7);
        check_eq(get_as<float>(x), 3e7f);
      }
    }
  }
  GIVEN("a config value x with value 123") {
    auto x = config_value{123};
    WHEN("using get_as with floating point types") {
      THEN("CAF converts the value") {
        check_eq(get_as<long double>(x), 123.0);
        check_eq(get_as<double>(x), 123.0);
        check_eq(get_as<float>(x), 123.f);
      }
    }
  }
  GIVEN("a config value x with type annotation 'float' and the value 50") {
    config_value x;
    x.as_dictionary().emplace("@type", "float");
    x.as_dictionary().emplace("value", 123.0);
    WHEN("using get_as with floating point types") {
      THEN("CAF parses the value and performs a bound check") {
        check_eq(get_as<long double>(x), 123.0);
        check_eq(get_as<double>(x), 123.0);
        check_eq(get_as<float>(x), 123.f);
      }
    }
  }
  GIVEN("config_values of null, URI, boolean, list or dictionary") {
    WHEN("using get_as with integer types") {
      THEN("conversion fails") {
        check_eq(get_as<int64_t>(cv_null), sec::conversion_failed);
        check_eq(get_as<int64_t>(cv_true), sec::conversion_failed);
        check_eq(get_as<int64_t>(cv_false), sec::conversion_failed);
        check_eq(get_as<int64_t>(cv_empty_uri), sec::conversion_failed);
        check_eq(get_as<int64_t>(cv_empty_list), sec::conversion_failed);
        check_eq(get_as<int64_t>(cv_empty_dict), sec::conversion_failed);
      }
    }
  }
}

SCENARIO("get_as can convert config values to timespans") {
  using namespace std::chrono_literals;
  GIVEN("a config value with value 42s") {
    auto x = config_value{timespan{42s}};
    WHEN("using get_as with timespan or string") {
      THEN("conversion succeeds") {
        check_eq(get_as<timespan>(x), timespan{42s});
        check_eq(get_as<std::string>(x), "42s");
      }
    }
    WHEN("using get_as with type other than timespan or string") {
      THEN("conversion fails") {
        check_eq(get_as<int64_t>(x), sec::conversion_failed);
        check_eq(get_as<double>(x), sec::conversion_failed);
        check_eq(get_as<uri>(x), sec::conversion_failed);
        check_eq(get_as<config_value::list>(x), sec::conversion_failed);
        check_eq(get_as<config_value::dictionary>(x), sec::conversion_failed);
      }
    }
  }
  GIVEN("a config value with value 0s") {
    auto x = config_value{timespan{0s}};
    WHEN("using get_as with timespan or string") {
      THEN("conversion succeeds") {
        check_eq(get_as<timespan>(x), timespan{0});
        check_eq(get_as<std::string>(x), "0s");
      }
    }
  }
}

SCENARIO("get_as can convert config values to strings") {
  using string = std::string;
  GIVEN("any config value") {
    WHEN("using get_as with string") {
      THEN("CAF renders the value as string") {
        check_eq(get_as<string>(cv_null), "null");
        check_eq(get_as<string>(cv_true), "true");
        check_eq(get_as<string>(cv_false), "false");
        check_eq(get_as<string>(cv_empty_list), "[]");
        check_eq(get_as<string>(cv_empty_dict), "{}");
        check_eq(get_as<string>(config_value{42}), "42");
        check_eq(get_as<string>(config_value{4.2}), "4.2");
        check_eq(get_as<string>(config_value{timespan{4}}), "4ns");
        check_eq(get_as<string>(cv_caf_uri), "https://actor-framework.org");
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
        if (check(static_cast<bool>(maybe_res))
            && check_eq(maybe_res->size(), 3u)) {
          auto& res = *maybe_res;
          check_eq(get_as<int>(res[0]), 1);
          check_eq(get_as<int>(res[1]), 2);
          check_eq(get_as<int>(res[2]), 3);
        }
      }
    }
    WHEN("using get_as with int vector") {
      THEN("conversion succeeds") {
        auto maybe_res = get_as<std::vector<int>>(x);
        if (check(static_cast<bool>(maybe_res))
            && check_eq(maybe_res->size(), 3u)) {
          auto& res = *maybe_res;
          check_eq(res[0], 1);
          check_eq(res[1], 2);
          check_eq(res[2], 3);
        }
      }
    }
  }
  GIVEN("a config value with value \"[1, 2, 3]\"") {
    auto x = config_value("[1, 2, 3]"s);
    WHEN("using get_as with list") {
      THEN("conversion succeeds") {
        auto maybe_res = get_as<list>(x);
        if (check(static_cast<bool>(maybe_res))
            && check_eq(maybe_res->size(), 3u)) {
          auto& res = *maybe_res;
          check_eq(get_as<int>(res[0]), 1);
          check_eq(get_as<int>(res[1]), 2);
          check_eq(get_as<int>(res[2]), 3);
        }
      }
    }
    WHEN("using get_as with int vector") {
      THEN("conversion succeeds") {
        auto maybe_res = get_as<std::vector<int>>(x);
        if (check(static_cast<bool>(maybe_res))
            && check_eq(maybe_res->size(), 3u)) {
          auto& res = *maybe_res;
          check_eq(res[0], 1);
          check_eq(res[1], 2);
          check_eq(res[2], 3);
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
    GIVEN("the config value " + to_string(x)) {
      WHEN("using get_as with config_value::dictionary") {
        THEN("conversion succeeds") {
          auto maybe_res = get_as<dictionary>(x);
          if (check(static_cast<bool>(maybe_res))
              && check_eq(maybe_res->size(), 3u)) {
            auto& res = *maybe_res;
            check_eq(get_as<int>(res["a"]), 1);
            check_eq(get_as<int>(res["b"]), 2);
            check_eq(get_as<int>(res["c"]), 3);
          }
        }
      }
      WHEN("using get_as with config_value::list") {
        THEN("CAF converts the dictionary to a list of lists") {
          auto maybe_res = get_as<list>(x);
          if (check(static_cast<bool>(maybe_res))
              && check_eq(maybe_res->size(), 3u)) {
            auto& res = *maybe_res;
            if (auto kvp = unbox(get_as<list>(res[0]));
                check_eq(kvp.size(), 2u)) {
              check_eq(get_as<std::string>(kvp[0]), "a");
              check_eq(get_as<int>(kvp[1]), 1);
            }
            if (auto kvp = unbox(get_as<list>(res[1]));
                check_eq(kvp.size(), 2u)) {
              check_eq(get_as<std::string>(kvp[0]), "b");
              check_eq(get_as<int>(kvp[1]), 2);
            }
            if (auto kvp = unbox(get_as<list>(res[2]));
                check_eq(kvp.size(), 2u)) {
              check_eq(get_as<std::string>(kvp[0]), "c");
              check_eq(get_as<int>(kvp[1]), 3);
            }
          }
        }
      }
      WHEN("using get_as with vector of tuple consisting of string, int") {
        THEN("CAF converts the dictionary to a list of tuples") {
          using kvp_t = std::tuple<std::string, int>;
          auto maybe_res = get_as<std::vector<kvp_t>>(x);
          log::test::debug("maybe_res: {}", maybe_res);
          if (check(static_cast<bool>(maybe_res))
              && check_eq(maybe_res->size(), 3u)) {
            auto& res = *maybe_res;
            check_eq(res[0], kvp_t("a", 1));
            check_eq(res[1], kvp_t("b", 2));
            check_eq(res[2], kvp_t("c", 3));
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
    GIVEN("the config value " + to_string(x)) {
      WHEN("using get_as with map of string to int") {
        THEN("conversion succeeds") {
          auto maybe_res = get_as<std::map<std::string, int>>(x);
          if (check(static_cast<bool>(maybe_res))
              && check_eq(maybe_res->size(), 3u)) {
            auto& res = *maybe_res;
            check_eq(res["1"], 1);
            check_eq(res["2"], 4);
            check_eq(res["3"], 9);
          }
        }
      }
      WHEN("using get_as with unordered_map of string to int") {
        THEN("conversion succeeds") {
          auto maybe_res = get_as<std::unordered_map<std::string, int>>(x);
          if (check(static_cast<bool>(maybe_res))
              && check_eq(maybe_res->size(), 3u)) {
            auto& res = *maybe_res;
            check_eq(res["1"], 1);
            check_eq(res["2"], 4);
            check_eq(res["3"], 9);
          }
        }
      }
      WHEN("using get_as with map of int to int") {
        THEN("conversion succeeds") {
          auto maybe_res = get_as<std::map<int, int>>(x);
          if (check(static_cast<bool>(maybe_res))
              && check_eq(maybe_res->size(), 3u)) {
            auto& res = *maybe_res;
            check_eq(res[1], 1);
            check_eq(res[2], 4);
            check_eq(res[3], 9);
          }
        }
      }
      WHEN("using get_as with unordered_map of int to int") {
        THEN("conversion succeeds") {
          auto maybe_res = get_as<std::unordered_map<int, int>>(x);
          if (check(static_cast<bool>(maybe_res))
              && check_eq(maybe_res->size(), 3u)) {
            auto& res = *maybe_res;
            check_eq(res[1], 1);
            check_eq(res[2], 4);
            check_eq(res[3], 9);
          }
        }
      }
    }
  }
}

SCENARIO("get_as can convert config values to custom types") {
  std::vector<std::pair<sec, std::string>> sec_values{
    {sec::request_timeout, "request_timeout"s},
    {sec::cannot_open_port, "cannot_open_port"s},
    {sec::runtime_error, "runtime_error"s}};
  for (const auto& [enum_val, str_val] : sec_values) {
    config_value x{str_val};
    GIVEN("the config value " + to_string(x)) {
      WHEN("using get_as with sec") {
        THEN("CAF picks up the custom inspect_value overload for conversion") {
          auto maybe_res = get_as<sec>(x);
          if (check(static_cast<bool>(maybe_res)))
            check_eq(*maybe_res, enum_val);
        }
      }
    }
  }
  config_value::dictionary my_request_dict;
  my_request_dict["a"] = config_value{10};
  my_request_dict["b"] = config_value{20};
  auto my_request_val = config_value{my_request_dict};
  GIVEN("the config value") {
    log::test::debug("my_request_val: {}", my_request_val);
    WHEN("using get_as with my_request") {
      THEN("CAF picks up the custom inspect overload for conversion") {
        auto maybe_res = get_as<my_request>(my_request_val);
        if (check(static_cast<bool>(maybe_res)))
          check_eq(*maybe_res, my_request(10, 20));
      }
    }
  }
  std::vector<config_value> obj_vals{config_value{my_request_val},
                                     config_value{config_value::dictionary{}},
                                     config_value{"{}"s}};
  for (auto& x : obj_vals) {
    GIVEN("the config value") {
      log::test::debug("obj_vals: {}", x);
      WHEN("using get_as with dummy_tag_type") {
        THEN("CAF only checks whether the config value is dictionary-ish") {
          check(static_cast<bool>(get_as<dummy_tag_type>(my_request_val)));
        }
      }
    }
  }
  std::vector<config_value> non_obj_vals{config_value{}, config_value{42},
                                         config_value{"[1,2,3]"s}};
  for (auto& x : non_obj_vals) {
    GIVEN("the config value") {
      log::test::debug("non_obj_vals: {}", x);
      WHEN("using get_as with dummy_tag_type") {
        THEN("conversion fails") {
          check_eq(get_as<dummy_tag_type>(x), sec::conversion_failed);
        }
      }
    }
  }
}

namespace {

struct i64_wrapper {
  int64_t value;
};

template <class Inspector>
bool inspect(Inspector& f, i64_wrapper& x) {
  return f.apply(x.value);
}

} // namespace

SCENARIO("get_or converts or returns a fallback value") {
  GIVEN("the config value 42") {
    auto x = config_value{42};
    WHEN("using get_or with type int") {
      THEN("CAF ignores the default value") {
        check_eq(get_or(x, 10), 42);
      }
    }
    WHEN("using get_or with type string") {
      THEN("CAF ignores the default value") {
        check_eq(get_or(x, "foo"s), "42"s);
      }
    }
    WHEN("using get_or with type bool") {
      THEN("CAF returns the default value") {
        check_eq(get_or(x, false), false);
      }
    }
    WHEN("using get_or with type of int span") {
      int fallback_arr[] = {10, 20, 30};
      auto fallback = make_span(fallback_arr);
      THEN("CAF returns the default value after converting it to int vector") {
        auto result = get_or(x, fallback);
        static_assert(std::is_same_v<decltype(result), std::vector<int>>);
        check_eq(result, std::vector<int>({10, 20, 30}));
      }
    }
    WHEN("using get_or with type i64_wrapper") {
      THEN("CAF returns i64_wrapper{42}") {
        auto result = get_or<i64_wrapper>(x, 10);
        check_eq(result.value, 42);
      }
    }
  }
  GIVEN("the config value 'hello world'") {
    auto x = config_value{"hello world"};
    WHEN("using get_or with type i64_wrapper") {
      THEN("CAF returns the fallback value") {
        auto result = get_or<i64_wrapper>(x, 10);
        check_eq(result.value, 10);
      }
    }
  }
}

SCENARIO("config values can default-construct all registered types") {
  auto from = [this](type_id_t id) {
    config_value result;
    if (auto err = result.default_construct(id))
      fail("default construction failed: {}", err);
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
        check_eq(from(type_id_v<int8_t>), config_value{0});
        check_eq(from(type_id_v<int16_t>), config_value{0});
        check_eq(from(type_id_v<int32_t>), config_value{0});
        check_eq(from(type_id_v<int64_t>), config_value{0});
        check_eq(from(type_id_v<uint8_t>), config_value{0});
        check_eq(from(type_id_v<uint16_t>), config_value{0});
        check_eq(from(type_id_v<uint32_t>), config_value{0});
        check_eq(from(type_id_v<uint64_t>), config_value{0});
      }
    }
    WHEN("calling default_construct for any floating point type") {
      THEN("the config value becomes config_value::real{0}") {
        check_eq(from(type_id_v<float>), config_value{0.0});
        check_eq(from(type_id_v<double>), config_value{0.0});
        check_eq(from(type_id_v<long double>), config_value{0.0});
      }
    }
    WHEN("calling default_construct for std::string") {
      THEN("the config value becomes \"\"") {
        check_eq(from(type_id_v<std::string>), config_value{std::string{}});
      }
    }
    WHEN("calling default_construct for caf::timespan") {
      THEN("the config value becomes 0s") {
        check_eq(from(type_id_v<timespan>), config_value{timespan{0}});
      }
    }
    WHEN("calling default_construct for caf::uri") {
      THEN("the config value becomes an empty URI") {
        check_eq(from(type_id_v<uri>), config_value{uri{}});
      }
    }
    WHEN("calling default_construct for any list-like type") {
      THEN("the config value becomes a config_value::list") {
        check_eq(from(type_id_v<std::vector<actor>>).get_data().index(), 7u);
        check_eq(from(type_id_v<std::vector<bool>>).get_data().index(), 7u);
      }
    }
    WHEN("calling default_construct for any custom non-list type") {
      THEN("the config value becomes a dictionary") {
        auto val = from(type_id_v<my_request>);
        check_eq(val.get_data().index(), 8u);
        auto& dict = val.as_dictionary();
        check_eq(keys(dict), std::vector<std::string>({"a", "b"}));
        check_eq(dict["a"].get_data().index(), 1u);
        check_eq(get_as<int32_t>(dict["a"]), 0);
        check_eq(dict["b"].get_data().index(), 1u);
        check_eq(get_as<int32_t>(dict["b"]), 0);
      }
    }
  }
}

#define CHECK_ROUNDTRIP(init_val, expected_str)                                \
  do {                                                                         \
    config_value x;                                                            \
    if (auto assign_failed = x.assign(init_val); check(!assign_failed)) {      \
      auto str = to_string(x);                                                 \
      check_eq(str, expected_str);                                             \
      auto parsed = config_value::parse(str);                                  \
      using init_val_type = decltype(init_val);                                \
      if (check(static_cast<bool>(parsed))) {                                  \
        if constexpr (!std::is_same_v<init_val_type, message>)                 \
          check_eq(get_as<init_val_type>(*parsed), init_val);                  \
        else                                                                   \
          check_eq(to_string(*parsed), str);                                   \
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
        CHECK_ROUNDTRIP(my_request(1, 2), R"_({a = 1, b = 2})_");
        CHECK_ROUNDTRIP(std::make_tuple(add_atom_v, 1, 2), R"_([{}, 1, 2])_");
        CHECK_ROUNDTRIP(make_message(add_atom_v, 1, 2), R"_([{}, 1, 2])_");
      }
    }
  }
}

SCENARIO("config values can convert lists of tuples to dictionaries") {
  GIVEN("a config value containing a list of key-value pairs (lists)") {
    WHEN("calling as_dictionary on the object") {
      THEN("the config value lifts the key-value pair list to a dictionary") {
        auto x = make_config_value_list(make_config_value_list("one", 1),
                                        make_config_value_list(2, "two"));
        auto& dict = x.as_dictionary();
        check_eq(dict.size(), 2u);
        check_eq(dict["one"], config_value{1});
        check_eq(dict["2"], config_value{"two"s});
      }
    }
  }
  GIVEN("a config value containing a string representing a kvp list") {
    WHEN("calling as_dictionary on the object") {
      THEN("the config value lifts the key-value pair list to a dictionary") {
        auto x = config_value{R"_([["one", 1], [2, "two"]])_"};
        auto& dict = x.as_dictionary();
        check_eq(dict.size(), 2u);
        check_eq(dict["one"], config_value{1});
        check_eq(dict["2"], config_value{"two"s});
      }
    }
  }
}

SCENARIO("config values can parse messages") {
  using testee_t = typed_actor<result<void>(int16_t),                     //
                               result<void>(int32_t, int32_t),            //
                               result<void>(my_request),                  //
                               result<void>(add_atom, int32_t, int32_t)>; //
  auto parse = [](std::string_view str) {
    testee_t testee;
    return config_value::parse_msg(str, testee);
  };
  GIVEN("a typed actor handle and valid input strings") {
    WHEN("calling config_value::parse_msg") {
      THEN("matching message types are generated") {
        if (auto msg = parse("16000"); check(static_cast<bool>(msg))) {
          if (check((msg->match_elements<int16_t>()))) {
            check_eq(msg->get_as<int16_t>(0), 16000);
          }
        }
        if (auto msg = parse("[16000]"); check(static_cast<bool>(msg))) {
          if (check((msg->match_elements<int16_t>()))) {
            check_eq(msg->get_as<int16_t>(0), 16000);
          }
        }
        if (auto msg = parse("[1, 2]"); check(static_cast<bool>(msg))) {
          if (check((msg->match_elements<int32_t, int32_t>()))) {
            check_eq(msg->get_as<int32_t>(0), 1);
            check_eq(msg->get_as<int32_t>(1), 2);
          }
        }
        if (auto msg = parse("{a = 1, b = 2}"); check(static_cast<bool>(msg))) {
          if (check((msg->match_elements<my_request>()))) {
            check_eq(msg->get_as<my_request>(0), my_request(1, 2));
          }
        }
        if (auto msg = parse("[{a = 1, b = 2}]");
            check(static_cast<bool>(msg))) {
          if (check((msg->match_elements<my_request>()))) {
            check_eq(msg->get_as<my_request>(0), my_request(1, 2));
          }
        }
        if (auto msg = parse(R"_([{}, 1, 2])_");
            check(static_cast<bool>(msg))) {
          if (check((msg->match_elements<add_atom, int32_t, int32_t>()))) {
            check_eq(msg->get_as<int32_t>(1), 1);
            check_eq(msg->get_as<int32_t>(2), 2);
          }
        }
      }
    }
  }
  GIVEN("a typed actor handle and invalid input strings") {
    WHEN("calling config_value::parse_msg") {
      THEN("nullopt is returned") {
        check(!parse("65000"));
        check(!parse("[1, 2, 3]"));
        check(!parse("[{a = 1.1, b = 2.2}]"));
      }
    }
  }
}

SCENARIO("config_value::parse returns an error for invalid inputs") {
  auto parse = [](const string& str) {
    if (auto x = config_value::parse(str))
      return error{};
    else
      return std::move(x.error());
  };
  GIVEN("malformed input strings") {
    WHEN("calling config_value::parse") {
      THEN("the parser error code is returned") {
        check_eq(parse("10msb"), pec::trailing_character);
        check_eq(parse("10foo"), pec::trailing_character);
        check_eq(parse("[1,"), pec::unexpected_eof);
        check_eq(parse("{a=,"), pec::unexpected_character);
        check_eq(parse("{a=1,"), pec::unexpected_eof);
        check_eq(parse("{a=1 b=2}"), pec::unexpected_character);
      }
    }
  }
}

SCENARIO("config_value::parse ignores trailing and leading whitespaces") {
  check_eq(config_value::parse(" 123  "), config_value{123});
  check_eq(config_value::parse(" hello world   "), config_value{"hello world"});
  check_eq(config_value::parse(""), config_value{""});
}

} // WITH_FIXTURE(fixture)

// -- end of scenario testing, here come several baseline checks for parsing ---

#define LIST_TEST(input_str, ...)                                              \
  ([&] {                                                                       \
    config_value cv{input_str};                                                \
    auto converted = cv.to_list();                                             \
    if (converted) {                                                           \
      auto res = make_config_value_list(__VA_ARGS__);                          \
      check_eq(*converted, get<config_value::list>(res));                      \
    } else {                                                                   \
      fail("{}", converted.error());                                           \
    }                                                                          \
  })()

TEST("list baseline testing") {
  auto ls = [](auto... xs) { return make_config_value_list(std::move(xs)...); };
  LIST_TEST(R"_([1, 2, 3])_", 1, 2, 3);
  LIST_TEST(R"_(1, 2, 3)_", 1, 2, 3);
  LIST_TEST(R"_([[1, 2], [3, 4]])_", ls(1, 2), ls(3, 4));
  LIST_TEST(R"_([1, 2], [3, 4])_", ls(1, 2), ls(3, 4));
  LIST_TEST(R"_([1, [2, [3, 4]]])_", 1, ls(2, ls(3, 4)));
}

#define DICT_TEST(input_str, ...)                                              \
  ([&] {                                                                       \
    config_value cv{input_str};                                                \
    auto converted = cv.to_dictionary();                                       \
    if (converted) {                                                           \
      auto res = config_value::dictionary{__VA_ARGS__};                        \
      check_eq(*converted, res);                                               \
    } else {                                                                   \
      fail("{}", converted.error());                                           \
    }                                                                          \
  })()

TEST("dictionary baseline testing") {
  using dict = config_value::dictionary;
  auto kvp = [](std::string key,
                auto val) -> std::pair<const std::string, config_value> {
    return {std::move(key), config_value{std::move(val)}};
  };
  auto ls = [](auto... xs) { return make_config_value_list(std::move(xs)...); };
  // Unquoted keys.
  DICT_TEST(R"_(a = 1, b = 2)_", kvp("a", 1), kvp("b", 2));
  DICT_TEST(R"_({a = 1, b = 2})_", kvp("a", 1), kvp("b", 2));
  DICT_TEST(R"_(my { app { foo = 'bar' } })_",
            kvp("my", dict{kvp("app", dict{kvp("foo", "bar")})}));
  // Quoted keys.
  DICT_TEST(R"_("a" = 1, "b" = 2)_", kvp("a", 1), kvp("b", 2));
  DICT_TEST(R"_({"a" = 1, "b" = 2})_", kvp("a", 1), kvp("b", 2));
  DICT_TEST(R"_("my" { "app" { "foo" = 'bar' } })_",
            kvp("my", dict{kvp("app", dict{kvp("foo", "bar")})}));
  DICT_TEST(R"_('a' = 1, "b" = 2)_", kvp("a", 1), kvp("b", 2));
  DICT_TEST(R"_({'a' = 1, 'b' = 2})_", kvp("a", 1), kvp("b", 2));
  DICT_TEST(R"_('my' { 'app' { 'foo' = "bar" } })_",
            kvp("my", dict{kvp("app", dict{kvp("foo", "bar")})}));
  // JSON notation.
  DICT_TEST(R"_({"a": 1, "b": 2})_", kvp("a", 1), kvp("b", 2));
  DICT_TEST(R"_({"my": { "app": { "foo": "bar" } }})_",
            kvp("my", dict{kvp("app", dict{kvp("foo", "bar")})}));
  // Key/value list.
  DICT_TEST(R"_(["a", 1], ["b", 2])_", kvp("a", 1), kvp("b", 2));
  DICT_TEST(R"_([["my", [ "app", [ "foo", "bar" ]]]])_",
            kvp("my", ls("app", ls("foo", "bar"))));
}

TEST_INIT() {
  caf::init_global_meta_objects<caf::id_block::config_value_test>();
}

} // namespace
