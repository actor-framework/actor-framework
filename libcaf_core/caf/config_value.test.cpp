// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/config_value.hpp"

#include "caf/test/approx.hpp"
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
using namespace std::literals;

namespace {

struct my_request;
struct dummy_tag_type;
struct failing_save_type;
struct u16string_type;

} // namespace

CAF_BEGIN_TYPE_ID_BLOCK(config_value_test, caf::first_custom_type_id + 250, 20)

  CAF_ADD_TYPE_ID(config_value_test, (my_request))
  CAF_ADD_TYPE_ID(config_value_test, (dummy_tag_type))
  CAF_ADD_TYPE_ID(config_value_test, (failing_save_type))
  CAF_ADD_TYPE_ID(config_value_test, (u16string_type))
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

constexpr bool operator==(dummy_tag_type, dummy_tag_type) noexcept {
  return true;
}

// Type that fails during save with a non-sec error to test error handling
struct failing_save_type {
  int32_t value = 0;
};

template <class Inspector>
bool inspect(Inspector& f, failing_save_type& x) {
  // For save operations, set a pec error to test non-sec error path
  if constexpr (Inspector::is_loading) {
    return f.object(x).fields(f.field("value", x.value));
  } else {
    // During save, set a pec error to test the else branch in default_construct
    f.emplace_error(pec::invalid_state, "test error");
    return false;
  }
}

// Type that contains a u16string to trigger sec error during save
struct u16string_type {
  std::u16string str;
};

template <class Inspector>
bool inspect(Inspector& f, u16string_type& x) {
  return f.object(x).fields(f.field("str", x.str));
}

template <class T>
T unbox(caf::expected<T> x) {
  if (!x)
    test::runnable::current().fail("{}", to_string(x.error()));
  return std::move(*x);
}

// helper function for serialization and deserializaiton roundtrip test
template <class T>
T roundtrip(T x) {
  config_value val;
  config_value_writer sink{&val};
  if (!sink.apply(x))
    test::runnable::current().fail("save failed");
  T y;
  config_value_reader source{&val};
  if (!source.apply(y))
    test::runnable::current().fail("load failed");
  return y;
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
        check_eq(get_as<bool>(cv_null),
                 expected<bool>{unexpect, sec::conversion_failed});
        check_eq(get_as<bool>(cv_empty_uri),
                 expected<bool>{unexpect, sec::conversion_failed});
        check_eq(get_as<bool>(cv_empty_list),
                 expected<bool>{unexpect, sec::conversion_failed});
        check_eq(get_as<bool>(cv_empty_dict),
                 expected<bool>{unexpect, sec::conversion_failed});
        check_eq(get_as<bool>(config_value{0}),
                 expected<bool>{unexpect, sec::conversion_failed});
        check_eq(get_as<bool>(config_value{1}),
                 expected<bool>{unexpect, sec::conversion_failed});
        check_eq(get_as<bool>(config_value{0.f}),
                 expected<bool>{unexpect, sec::conversion_failed});
        check_eq(get_as<bool>(config_value{1.f}),
                 expected<bool>{unexpect, sec::conversion_failed});
        check_eq(get_as<bool>(config_value{""s}),
                 expected<bool>{unexpect, sec::conversion_failed});
        check_eq(get_as<bool>(config_value{"1"s}),
                 expected<bool>{unexpect, sec::conversion_failed});
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
        check_eq(get_as<int16_t>(x),
                 expected<int16_t>{unexpect, sec::conversion_failed});
        check_eq(get_as<uint8_t>(x),
                 expected<uint8_t>{unexpect, sec::conversion_failed});
        check_eq(get_as<int8_t>(x),
                 expected<int8_t>{unexpect, sec::conversion_failed});
      }
    }
  }
  GIVEN("a config value x with value -5") {
    auto x = config_value{-5};
    WHEN("using get_as with integer types") {
      THEN("conversion fails for all unsigned types") {
        check_eq(get_as<uint64_t>(x),
                 expected<uint64_t>{unexpect, sec::conversion_failed});
        check_eq(get_as<int64_t>(x), -5);
        check_eq(get_as<uint32_t>(x),
                 expected<uint32_t>{unexpect, sec::conversion_failed});
        check_eq(get_as<int32_t>(x), -5);
        check_eq(get_as<uint16_t>(x),
                 expected<uint16_t>{unexpect, sec::conversion_failed});
        check_eq(get_as<int16_t>(x), -5);
        check_eq(get_as<uint8_t>(x),
                 expected<uint8_t>{unexpect, sec::conversion_failed});
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
        check_eq(get_as<int16_t>(x),
                 expected<int16_t>{unexpect, sec::conversion_failed});
        check_eq(get_as<uint8_t>(x),
                 expected<uint8_t>{unexpect, sec::conversion_failed});
        check_eq(get_as<int8_t>(x),
                 expected<int8_t>{unexpect, sec::conversion_failed});
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
        check_eq(get_as<uint64_t>(x),
                 expected<uint64_t>{unexpect, sec::conversion_failed});
        check_eq(get_as<int64_t>(x),
                 expected<int64_t>{unexpect, sec::conversion_failed});
        check_eq(get_as<uint32_t>(x),
                 expected<uint32_t>{unexpect, sec::conversion_failed});
        check_eq(get_as<int32_t>(x),
                 expected<int32_t>{unexpect, sec::conversion_failed});
        check_eq(get_as<uint16_t>(x),
                 expected<uint16_t>{unexpect, sec::conversion_failed});
        check_eq(get_as<int16_t>(x),
                 expected<int16_t>{unexpect, sec::conversion_failed});
        check_eq(get_as<uint8_t>(x),
                 expected<uint8_t>{unexpect, sec::conversion_failed});
        check_eq(get_as<int8_t>(x),
                 expected<int8_t>{unexpect, sec::conversion_failed});
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
        check_eq(get_as<uint64_t>(x),
                 expected<uint64_t>{unexpect, sec::conversion_failed});
        check_eq(get_as<int64_t>(x),
                 expected<int64_t>{unexpect, sec::conversion_failed});
        check_eq(get_as<uint32_t>(x),
                 expected<uint32_t>{unexpect, sec::conversion_failed});
        check_eq(get_as<int32_t>(x),
                 expected<int32_t>{unexpect, sec::conversion_failed});
        check_eq(get_as<uint16_t>(x),
                 expected<uint16_t>{unexpect, sec::conversion_failed});
        check_eq(get_as<int16_t>(x),
                 expected<int16_t>{unexpect, sec::conversion_failed});
        check_eq(get_as<uint8_t>(x),
                 expected<uint8_t>{unexpect, sec::conversion_failed});
        check_eq(get_as<int8_t>(x),
                 expected<int8_t>{unexpect, sec::conversion_failed});
      }
    }
  }
  GIVEN("config_values of null, URI, boolean, list or dictionary") {
    WHEN("using get_as with floating point types") {
      THEN("conversion fails") {
        check_eq(get_as<float>(cv_null),
                 expected<float>{unexpect, sec::conversion_failed});
        check_eq(get_as<float>(cv_true),
                 expected<float>{unexpect, sec::conversion_failed});
        check_eq(get_as<float>(cv_false),
                 expected<float>{unexpect, sec::conversion_failed});
        check_eq(get_as<float>(cv_empty_uri),
                 expected<float>{unexpect, sec::conversion_failed});
        check_eq(get_as<float>(cv_empty_list),
                 expected<float>{unexpect, sec::conversion_failed});
        check_eq(get_as<float>(cv_empty_dict),
                 expected<float>{unexpect, sec::conversion_failed});
        check_eq(get_as<double>(cv_null),
                 expected<double>{unexpect, sec::conversion_failed});
        check_eq(get_as<double>(cv_true),
                 expected<double>{unexpect, sec::conversion_failed});
        check_eq(get_as<double>(cv_false),
                 expected<double>{unexpect, sec::conversion_failed});
        check_eq(get_as<double>(cv_empty_uri),
                 expected<double>{unexpect, sec::conversion_failed});
        check_eq(get_as<double>(cv_empty_list),
                 expected<double>{unexpect, sec::conversion_failed});
        check_eq(get_as<double>(cv_empty_dict),
                 expected<double>{unexpect, sec::conversion_failed});
        check_eq(get_as<long double>(cv_null),
                 expected<long double>{unexpect, sec::conversion_failed});
        check_eq(get_as<long double>(cv_true),
                 expected<long double>{unexpect, sec::conversion_failed});
        check_eq(get_as<long double>(cv_false),
                 expected<long double>{unexpect, sec::conversion_failed});
        check_eq(get_as<long double>(cv_empty_uri),
                 expected<long double>{unexpect, sec::conversion_failed});
        check_eq(get_as<long double>(cv_empty_list),
                 expected<long double>{unexpect, sec::conversion_failed});
        check_eq(get_as<long double>(cv_empty_dict),
                 expected<long double>{unexpect, sec::conversion_failed});
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
        check_eq(get_as<float>(x),
                 expected<float>{unexpect, sec::conversion_failed});
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
        check_eq(get_as<int64_t>(cv_null),
                 expected<int64_t>{unexpect, sec::conversion_failed});
        check_eq(get_as<int64_t>(cv_true),
                 expected<int64_t>{unexpect, sec::conversion_failed});
        check_eq(get_as<int64_t>(cv_false),
                 expected<int64_t>{unexpect, sec::conversion_failed});
        check_eq(get_as<int64_t>(cv_empty_uri),
                 expected<int64_t>{unexpect, sec::conversion_failed});
        check_eq(get_as<int64_t>(cv_empty_list),
                 expected<int64_t>{unexpect, sec::conversion_failed});
        check_eq(get_as<int64_t>(cv_empty_dict),
                 expected<int64_t>{unexpect, sec::conversion_failed});
      }
    }
  }
}

SCENARIO("get_as can convert config values to timespans") {
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
        check_eq(get_as<int64_t>(x),
                 expected<int64_t>{unexpect, sec::conversion_failed});
        check_eq(get_as<double>(x),
                 expected<double>{unexpect, sec::conversion_failed});
        check_eq(get_as<uri>(x),
                 expected<uri>{unexpect, sec::conversion_failed});
        check_eq(get_as<config_value::list>(x),
                 expected<config_value::list>{unexpect,
                                              sec::conversion_failed});
        check_eq(get_as<config_value::dictionary>(x),
                 expected<config_value::dictionary>{unexpect,
                                                    sec::conversion_failed});
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
        THEN("CAF picks up the custom inspect_value overload for "
             "conversion") {
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
          check_eq(get_as<dummy_tag_type>(x),
                   expected<dummy_tag_type>{unexpect, sec::conversion_failed});
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
      auto fallback = std::span{fallback_arr, std::size(fallback_arr)};
      THEN("CAF returns the default value after converting it to int "
           "vector") {
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
      THEN("the config value lifts the key-value pair list to a "
           "dictionary") {
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
      THEN("the config value lifts the key-value pair list to a "
           "dictionary") {
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

TEST("serialization and deserialization roundtrip for config_value") {
  check_eq(roundtrip(int8_t{42}), int8_t{42});
  check_eq(roundtrip(int8_t{-42}), int8_t{-42});
  check_eq(roundtrip(int16_t{42}), int16_t{42});
  check_eq(roundtrip(int16_t{-42}), int16_t{-42});
  check_eq(roundtrip(int32_t{42}), int32_t{42});
  check_eq(roundtrip(int32_t{-42}), int32_t{-42});
  check_eq(roundtrip(int64_t{42}), int64_t{42});
  check_eq(roundtrip(int64_t{-42}), int64_t{-42});
  check_eq(roundtrip(uint8_t{42}), uint8_t{42});
  check_eq(roundtrip(uint16_t{42}), uint16_t{42});
  check_eq(roundtrip(uint32_t{42}), uint32_t{42});
  check_eq(roundtrip(uint64_t{42}), uint64_t{42});
  check_eq(roundtrip(42), 42);
  check_eq(roundtrip(42.0), test::approx{42.0});
  check_eq(roundtrip(42.0f), test::approx{42.0f});
  check_eq(roundtrip(42s), 42s);
  check_eq(roundtrip("hello world"s), "hello world"s);
  check_eq(roundtrip(std::vector<int>{1, 2, 3}), std::vector<int>{1, 2, 3});
  check_eq(roundtrip(std::vector<bool>{true, false, true}),
           std::vector<bool>{true, false, true});
  check_eq(roundtrip(std::map<string, int>{{"one", 1}, {"two", 2}}),
           std::map<string, int>{{"one", 1}, {"two", 2}});
  check_eq(roundtrip(std::unordered_map<string, int>{{"one", 1}, {"two", 2}}),
           std::unordered_map<string, int>{{"one", 1}, {"two", 2}});
  check_eq(roundtrip(my_request{1, 2}), my_request{1, 2});
}

TEST("config_value handles equality operators") {
  config_value x{42.0};
  config_value y{42.0};
  config_value z{43.0};
  check_eq(x, y);
  check_ne(x, z);
  check_ge(z, y);
  check_gt(z, y);
  check_le(y, z);
  check_lt(y, z);
}

TEST("config_value can get type_id") {
  check_eq(config_value{42}.type_id(), type_id_v<int64_t>);
  check_eq(config_value{42.0}.type_id(), type_id_v<double>);
  check_eq(config_value{"hello world"}.type_id(), type_id_v<std::string>);
  check_eq(config_value{timespan{42s}}.type_id(), type_id_v<timespan>);
  check_eq(config_value{uri{}}.type_id(), type_id_v<uri>);
  check_eq(config_value{true}.type_id(), type_id_v<bool>);
  check_eq(config_value{false}.type_id(), type_id_v<bool>);
  check_eq(config_value{config_value::list{}}.type_id(),
           type_id_v<config_value::list>);
  check_eq(config_value{config_value::dictionary{}}.type_id(),
           type_id_v<config_value::dictionary>);
}

TEST("config_value parse trims leading and trailing whitespace") {
  SECTION("spaces only") {
    auto result = config_value::parse("   ");
    check(result.has_value());
    check_eq(*result, config_value{""});
  }
  SECTION("spaces and tabs") {
    auto result = config_value::parse("  \t  ");
    check(result.has_value());
    check_eq(*result, config_value{""});
  }
  SECTION("empty string") {
    auto result = config_value::parse("");
    check(result.has_value());
    check_eq(*result, config_value{""});
  }
}

TEST("config_value::append adds elements to lists") {
  SECTION("appending to nil creates a new list with the element") {
    config_value x;
    x.append(config_value{1});
    if (check_eq(x.get_data().index(), 7u)) {
      auto lst = x.to_list();
      if (check_eq(lst->size(), 1u)) {
        check_eq((*lst)[0], config_value{1});
      }
    }
  }
  SECTION("appending to non-list converts to list") {
    config_value x{42};
    x.append(config_value{100});
    x.append(config_value{"test"});
    auto lst = x.to_list();
    check(lst.has_value());
    if (check_eq(lst->size(), 3u)) {
      check_eq((*lst)[0], config_value{42});
      check_eq((*lst)[1], config_value{100});
      check_eq((*lst)[2], config_value{"test"});
    }
  }
  SECTION("appending to existing list") {
    config_value y{config_value::list{
      config_value{1},
      config_value{2},
      config_value{3},
    }};
    y.append(config_value{4});
    auto lst = y.to_list();
    if (check_eq(lst->size(), 4u)) {
      check_eq((*lst)[3], config_value{4});
    }
  }
}

TEST("config_value signed_index returns index as ptrdiff_t") {
  check_eq(config_value{}.signed_index(), 0);
  check_eq(config_value{42}.signed_index(), 1);
  check_eq(config_value{true}.signed_index(), 2);
  check_eq(config_value{3.14}.signed_index(), 3);
  check_eq(config_value{timespan{1s}}.signed_index(), 4);
  check_eq(config_value{uri{}}.signed_index(), 5);
  check_eq(config_value{"test"}.signed_index(), 6);
  check_eq(config_value{config_value::list{}}.signed_index(), 7);
  check_eq(config_value{config_value::dictionary{}}.signed_index(), 8);
}

TEST("config_value default_construct") {
  SECTION("bool type") {
    config_value x;
    auto err = x.default_construct(type_id_v<bool>);
    check(!err);
    check_eq(x, config_value{false});
  }
  SECTION("unknown type") {
    config_value x;
    type_id_t unknown_id = static_cast<type_id_t>(0xFFFF);
    auto err = x.default_construct(unknown_id);
    check(static_cast<bool>(err));
    check_eq(err.value(), sec::unknown_type);
  }
  SECTION("type with u16string fails with sec error") {
    // u16string_type contains a u16string field which will fail during save
    // because u16string is not supported, triggering sec error path
    config_value x;
    auto err = x.default_construct(type_id_v<u16string_type>);
    check(static_cast<bool>(err));
    // The error should be sec::runtime_error (from config_value_writer)
    check_eq(err.value(), sec::runtime_error);
  }
  SECTION(
    "type with failing save returns conversion_failed for non-sec error") {
    // failing_save_type sets a pec error during save, which is not sec category
    // This tests the else branch that returns sec::conversion_failed
    config_value x;
    auto err = x.default_construct(type_id_v<failing_save_type>);
    check(static_cast<bool>(err));
    // Since the error category is pec (not sec), it should return
    // conversion_failed
    check_eq(err.value(), sec::conversion_failed);
  }
}

namespace {

config_value make_typed_config_value(std::string_view type) {
  config_value x;
  x.as_dictionary().emplace("@type", type);
  return x;
}

config_value make_typed_config_value(std::string_view type,
                                     config_value value) {
  config_value x;
  x.as_dictionary().emplace("@type", type);
  x.as_dictionary().emplace("value", value);
  return x;
}

} // namespace

TEST("invalid conversions return sec::conversion_failed") {
  SECTION("to_boolean") {
    auto x = make_typed_config_value("bool");
    check_eq(x.to_boolean(), error_code{sec::conversion_failed});
  }
  SECTION("to_integer") {
    auto x = make_typed_config_value("int32_t");
    check_eq(x.to_integer(), error_code{sec::conversion_failed});
  }
  SECTION("to_real") {
    auto x = make_typed_config_value("double");
    check_eq(x.to_real(), error_code{sec::conversion_failed});
  }
  SECTION("to_dictionary") {
    auto x = config_value{"not a valid dictionary string"};
    check_eq(x.to_dictionary(), error_code{sec::conversion_failed});
  }
}

TEST("config_value to_list converts dictionary to list") {
  config_value x;
  x.as_dictionary().emplace("a", 1);
  x.as_dictionary().emplace("b", 2);
  x.as_dictionary().emplace("c", 3);
  auto lst = x.to_list();
  check(lst.has_value());
  check_eq(lst->size(), 3u);
  for (const auto& elem : *lst) {
    auto pair = elem.to_list();
    check(pair.has_value());
    check_eq(pair->size(), 2u);
  }
}

TEST("config_value to_list converts string dictionary to list") {
  config_value x{R"_({"a": 1, "b": 2})_"};
  check(holds_alternative<std::string>(x.get_data()));
  SECTION("string can be parsed as dictionary") {
    config_value::dictionary test_dict;
    std::string str = get<std::string>(x.get_data());
    auto parse_err = detail::parse(str, test_dict);
    check(parse_err == none);
    check_eq(test_dict.size(), 2u);
  }
  SECTION("to_list parses dictionary from string") {
    auto lst = x.to_list();
    check(lst.has_value());
    check_eq(lst->size(), 2u);
    for (const auto& elem : *lst) {
      auto pair = elem.to_list();
      check(pair.has_value());
      check_eq(pair->size(), 2u);
    }
  }
}

TEST("config_value to_dictionary requires lists to have two elements") {
  SECTION("list with one element") {
    config_value x{config_value::list{config_value{42}}};
    check_eq(x.to_dictionary(), error_code{sec::conversion_failed});
  }
  SECTION("list with two elements") {
    config_value x{config_value::list{config_value{config_value::list{
      config_value{"key"},
      config_value{"value"},
    }}}};
    auto maybe_dict = x.to_dictionary();
    if (check_has_value(maybe_dict)) {
      auto& dict = *maybe_dict;
      check_eq(dict.size(), 1u);
      check_eq(dict["key"], config_value{"value"});
    }
  }
  SECTION("list with three elements") {
    config_value x{config_value::list{config_value{config_value::list{
      config_value{1},
      config_value{2},
      config_value{3},
    }}}};
    check_eq(x.to_dictionary(), error_code{sec::conversion_failed});
  }
}

TEST("config_value can_convert_to_dictionary checks string conversion") {
  SECTION("valid dictionary string") {
    config_value valid_dict_str{R"_({"a": 1, "b": 2})_"};
    check(valid_dict_str.can_convert_to_dictionary());
  }
  SECTION("invalid string") {
    config_value invalid_str{"not a dictionary"};
    check(!invalid_str.can_convert_to_dictionary());
  }
  SECTION("already a dictionary") {
    config_value already_dict;
    already_dict.as_dictionary().emplace("a", 1);
    check(already_dict.can_convert_to_dictionary());
  }
  SECTION("not a string") {
    config_value not_string{42};
    check(!not_string.can_convert_to_dictionary());
  }
}

TEST("config_value operator<< outputs string representation") {
  SECTION("integer") {
    config_value x{42};
    std::ostringstream oss;
    oss << x;
    check_eq(oss.str(), "42");
  }
  SECTION("string") {
    config_value y{"hello"};
    std::ostringstream oss;
    oss << y;
    check_eq(oss.str(), "hello");
  }
  SECTION("list") {
    config_value z{config_value::list{
      config_value{1},
      config_value{2},
      config_value{3},
    }};
    std::ostringstream oss;
    oss << z;
    check(oss.str().find("1") != std::string::npos);
  }
}

TEST("config_value to_timespan handles string conversion") {
  SECTION("valid timespan string") {
    config_value x{"42s"};
    auto result = x.to_timespan();
    check(result.has_value());
    check_eq(*result, timespan{42s});
  }
  SECTION("invalid timespan string") {
    config_value x{"not a valid timespan"};
    auto result = x.to_timespan();
    check(!result.has_value());
    check_eq(result.error().code(),
             static_cast<uint8_t>(sec::conversion_failed));
  }
}

TEST("config_value to_uri handles string conversion") {
  config_value x{"http://example.com"};
  auto result = x.to_uri();
  check(result.has_value());
  check_eq(result->str(), "http://example.com");
}

TEST("config_value to_boolean handles dictionary with wrong @type") {
  auto x = make_typed_config_value("int32_t", config_value{42});
  auto result = x.to_boolean();
  check(!result.has_value());
  check_eq(result.error().code(), static_cast<uint8_t>(sec::conversion_failed));
}

TEST("config_value to_integer handles dictionary with wrong @type") {
  auto x = make_typed_config_value("bool", config_value{true});
  auto result = x.to_integer();
  check(!result.has_value());
  check_eq(result.error().code(), static_cast<uint8_t>(sec::conversion_failed));
}

TEST("config_value to_real handles invalid string conversion") {
  config_value x{"not a number"};
  auto result = x.to_real();
  check(!result.has_value());
  check_eq(result.error().code(), static_cast<uint8_t>(sec::conversion_failed));
}

TEST("config_value to_real handles dictionary with wrong @type") {
  auto x = make_typed_config_value("bool", config_value{true});
  auto result = x.to_real();
  check(!result.has_value());
  check_eq(result.error().code(), static_cast<uint8_t>(sec::conversion_failed));
}

TEST("config_value to_uri handles uri type directly") {
  uri test_uri = make_uri("http://example.com").value();
  config_value x{test_uri};
  auto result = x.to_uri();
  check(result.has_value());
  check_eq(result->str(), "http://example.com");
}

TEST("config_value to_string handles escaped strings") {
  SECTION("string in list") {
    config_value list_with_string{
      config_value::list{config_value{"string with \"quotes\""}}};
    auto list_str = to_string(list_with_string);
    check(list_str.find("string with \\\"quotes\\\"") != std::string::npos);
  }
  SECTION("string in dictionary") {
    config_value dict;
    dict.as_dictionary().emplace("key", "value with \"quotes\"");
    auto dict_str = to_string(dict);
    check(dict_str.find("key") != std::string::npos);
  }
}

TEST("config_value to_string handles dictionary with non-alphanumeric "
     "keys") {
  config_value x;
  x.as_dictionary().emplace("key with spaces", 42);
  x.as_dictionary().emplace("key-with-dashes", 100);
  x.as_dictionary().emplace("key.with.dots", 200);
  auto str = to_string(x);
  check(str.find("key with spaces") != std::string::npos);
  check(str.find("key-with-dashes") != std::string::npos);
  check(str.find("key.with.dots") != std::string::npos);
}

TEST("config_value to_string for settings") {
  SECTION("non-empty settings") {
    settings xs;
    xs.emplace("key1", 42);
    xs.emplace("key2", "value");
    xs.emplace("key3", config_value::list{
                         config_value{1},
                         config_value{2},
                         config_value{3},
                       });
    std::string str = to_string(xs);
    check(str.find("key1") != std::string::npos);
    check(str.find("key2") != std::string::npos);
    check(str.find("key3") != std::string::npos);
  }
  SECTION("empty settings") {
    settings empty;
    std::string empty_str = to_string(empty);
    check_eq(empty_str, "{}");
  }
  SECTION("nested dictionaries") {
    settings xs;
    xs.emplace("key1", 42);
    settings nested;
    nested.emplace("outer", xs);
    std::string nested_str = to_string(nested);
    check(nested_str.find("outer") != std::string::npos);
  }
}

TEST("config_value assign with const char*") {
  config_value x;
  const char* str = "test string";
  auto err = x.assign(str);
  check(!err.valid());
  check_eq(x, config_value{"test string"});
}

TEST("config_value assign with config_value") {
  SECTION("rvalue") {
    config_value x;
    config_value y{42};
    auto err = x.assign(std::move(y));
    check(!err.valid());
    check_eq(x, config_value{42});
  }
  SECTION("lvalue") {
    config_value x;
    config_value z{100};
    auto err = x.assign(z);
    check(!err.valid());
    check_eq(x, config_value{100});
  }
}

TEST("config_value get_as for tuple with wrong element types") {
  config_value x{
    config_value::list{config_value{"not an int"}, config_value{"not an int"}}};
  auto result = get_as<std::tuple<int32_t, int32_t>>(x);
  check(!result.has_value());
  check_eq(result.error().code(), static_cast<uint8_t>(sec::conversion_failed));
}

TEST("config_value get_as for tuple with wrong number of arguments") {
  config_value x{config_value::list{config_value{1}}};
  auto result = get_as<std::tuple<int32_t, int32_t>>(x);
  check(!result.has_value());
  check_eq(result.error().code(), static_cast<uint8_t>(sec::conversion_failed));
}

TEST("config_value get_as for tuple with conversion error") {
  config_value x{42};
  auto result = get_as<std::tuple<int32_t, int32_t>>(x);
  check(!result.has_value());
  check_eq(result.error().code(), static_cast<uint8_t>(sec::conversion_failed));
}

TEST("config_value get_as for map with ambiguous keys") {
  config_value x;
  x.as_dictionary().emplace("1", 10);
  x.as_dictionary().emplace("1.0", 20);
  auto result = get_as<std::map<int32_t, int32_t>>(x);
  check(!result.has_value());
  check_eq(result.error().code(), static_cast<uint8_t>(sec::conversion_failed));
}

TEST("config_value get_as for map with value conversion failure") {
  config_value x;
  x.as_dictionary().emplace("key", "not an int");
  auto result = get_as<std::map<std::string, int32_t>>(x);
  check(!result.has_value());
  check_eq(result.error().code(), static_cast<uint8_t>(sec::conversion_failed));
}

TEST("config_value get_as for map with key conversion failure") {
  config_value x;
  x.as_dictionary().emplace("key", 42);
  auto result = get_as<std::map<int32_t, int32_t>>(x);
  check(!result.has_value());
  check_eq(result.error().code(), static_cast<uint8_t>(sec::conversion_failed));
}

TEST("config_value get_as for map with to_dictionary error") {
  config_value x{42};
  auto result = get_as<std::map<std::string, int32_t>>(x);
  check(!result.has_value());
  check_eq(result.error().code(), static_cast<uint8_t>(sec::conversion_failed));
}

TEST("config_value get_as for list with insert") {
  config_value x{
    config_value::list{config_value{1}, config_value{2}, config_value{3}}};
  SECTION("set uses insert") {
    auto result = get_as<std::set<int32_t>>(x);
    check(result.has_value());
    check_eq(result->size(), 3u);
    check(result->find(1) != result->end());
    check(result->find(2) != result->end());
    check(result->find(3) != result->end());
  }
  SECTION("list uses insert") {
    auto result = get_as<std::list<int32_t>>(x);
    check(result.has_value());
    check_eq(result->size(), 3u);
  }
}

TEST("config_value get_as for list with conversion error") {
  SECTION("vector with invalid element") {
    config_value x{config_value::list{
      config_value{1}, config_value{"not an int"}, config_value{3}}};
    auto result = get_as<std::vector<int32_t>>(x);
    check(!result.has_value());
    check_eq(result.error().code(),
             static_cast<uint8_t>(sec::conversion_failed));
  }
  SECTION("set with invalid element") {
    config_value y{
      config_value::list{config_value{1}, config_value{"not an int"}}};
    auto result = get_as<std::set<int32_t>>(y);
    check(!result.has_value());
    check_eq(result.error().code(),
             static_cast<uint8_t>(sec::conversion_failed));
  }
}

TEST("config_value lift for iterable types") {
  SECTION("vector") {
    std::vector<int> vec{1, 2, 3};
    config_value x{vec};
    auto lst = x.to_list();
    check(lst.has_value());
    check_eq(lst->size(), 3u);
  }
  SECTION("set") {
    std::set<std::string> set{"a", "b", "c"};
    config_value y{set};
    auto lst = y.to_list();
    check_eq(lst->size(), 3u);
  }
  SECTION("list") {
    std::list<int> list_val{10, 20, 30};
    config_value z{list_val};
    auto lst = z.to_list();
    check_eq(lst->size(), 3u);
  }
}

TEST("config_value variant_inspector_traits type_index") {
  config_value x{42};
  auto idx = variant_inspector_traits<config_value>::type_index(x);
  check_eq(idx, 1u);
}

TEST("config_value variant_inspector_traits visit") {
  config_value x{42};
  bool visited = false;
  variant_inspector_traits<config_value>::visit(
    [&visited](auto&&) { visited = true; }, x);
  check(visited);
}

TEST("config_value variant_inspector_traits assign") {
  config_value x;
  variant_inspector_traits<config_value>::assign(x, 42);
  check_eq(x, config_value{42});
}

TEST("config_value variant_inspector_traits load for all types") {
  SECTION("none_t") {
    bool called = false;
    variant_inspector_traits<config_value>::load(
      type_id_v<none_t>, [&called](auto&&) { called = true; });
    check(called);
  }
  SECTION("integer") {
    bool called = false;
    variant_inspector_traits<config_value>::load(
      type_id_v<config_value::integer>, [&called](auto&&) { called = true; });
    check(called);
  }
  SECTION("boolean") {
    bool called = false;
    variant_inspector_traits<config_value>::load(
      type_id_v<config_value::boolean>, [&called](auto&&) { called = true; });
    check(called);
  }
  SECTION("real") {
    bool called = false;
    variant_inspector_traits<config_value>::load(
      type_id_v<config_value::real>, [&called](auto&&) { called = true; });
    check(called);
  }
  SECTION("timespan") {
    bool called = false;
    variant_inspector_traits<config_value>::load(
      type_id_v<timespan>, [&called](auto&&) { called = true; });
    check(called);
  }
  SECTION("uri") {
    bool called = false;
    variant_inspector_traits<config_value>::load(
      type_id_v<uri>, [&called](auto&&) { called = true; });
    check(called);
  }
  SECTION("string") {
    bool called = false;
    variant_inspector_traits<config_value>::load(
      type_id_v<config_value::string>, [&called](auto&&) { called = true; });
    check(called);
  }
  SECTION("list") {
    bool called = false;
    variant_inspector_traits<config_value>::load(
      type_id_v<config_value::list>, [&called](auto&&) { called = true; });
    check(called);
  }
  SECTION("dictionary") {
    bool called = false;
    variant_inspector_traits<config_value>::load(
      type_id_v<config_value::dictionary>,
      [&called](auto&&) { called = true; });
    check(called);
  }
  SECTION("unknown type") {
    auto unknown_called = variant_inspector_traits<config_value>::load(
      type_id_v<my_request>, [](auto&&) {});
    check(!unknown_called);
  }
}

TEST_INIT() {
  caf::init_global_meta_objects<caf::id_block::config_value_test>();
}
