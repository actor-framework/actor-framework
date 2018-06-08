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

#include "caf/config.hpp"

#define CAF_SUITE config_value
#include "caf/test/unit_test.hpp"

#include <string>

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/atom.hpp"
#include "caf/deep_to_string.hpp"
#include "caf/detail/bounds_checker.hpp"
#include "caf/none.hpp"
#include "caf/variant.hpp"

using namespace std;
using namespace caf;

namespace {

using list = config_value::list;

using dictionary = config_value::dictionary;

struct dictionary_builder {
  dictionary dict;

  dictionary_builder&& add(const char* key, config_value value) && {
    dict.emplace(key, std::move(value));
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

template <class T>
detail::enable_if_t<std::is_integral<T>::value
                    && !std::is_same<T, typename config_value::integer>::value,
                    optional<T>>
get_if(const config_value* x) {
  using cvi = typename config_value::integer;
  auto ptr = config_value_access<cvi>::get_if(x);
  if (ptr && detail::bounds_checker<T>::check(*ptr))
    return static_cast<T>(*ptr);
  return none;
}

template <>
optional<uint64_t> get_if<uint64_t>(const config_value* x) {
  auto ptr = get_if<typename config_value::integer>(x);
  if (ptr && *ptr >= 0)
    return static_cast<uint64_t>(*ptr);
  return none;
}

template <class T>
detail::enable_if_t<std::is_integral<T>::value
                    && !std::is_same<T, typename config_value::integer>::value,
                    T>
get(const config_value& x) {
  auto res = get_if<T>(&x);
  if (res)
    return *res;
  CAF_RAISE_ERROR("invalid type found");
}

} // namespace <anonymous>

CAF_TEST(default_constructed) {
  config_value x;
  CAF_CHECK_EQUAL(holds_alternative<int64_t>(x), true);
  CAF_CHECK_EQUAL(get<int64_t>(x), 0);
  CAF_CHECK_EQUAL(x.type_name(), config_value::type_name_of<int64_t>());
}

CAF_TEST(integer) {
  config_value x{4200};
  CAF_CHECK_EQUAL(get<int64_t>(x), 4200);
  CAF_CHECK_EQUAL(get<size_t>(x), 4200u);
  CAF_CHECK_EQUAL(get_if<uint8_t>(&x), caf::none);
}

CAF_TEST(list) {
  using integer_list = std::vector<int64_t>;
  auto xs = make_config_value_list(1, 2, 3);
  CAF_CHECK_EQUAL(to_string(xs), "[1, 2, 3]");
  CAF_CHECK_EQUAL(xs.type_name(), config_value::type_name_of<list>());
  CAF_CHECK_EQUAL(holds_alternative<config_value::list>(xs), true);
  CAF_CHECK_EQUAL(holds_alternative<integer_list>(xs), true);
  CAF_CHECK_EQUAL(get<integer_list>(xs), integer_list({1, 2, 3}));
}

CAF_TEST(convert_to_list) {
  config_value x{int64_t{42}};
  CAF_CHECK_EQUAL(x.type_name(), config_value::type_name_of<int64_t>());
  CAF_CHECK_EQUAL(to_string(x), "42");
  x.convert_to_list();
  CAF_CHECK_EQUAL(x.type_name(), config_value::type_name_of<list>());
  CAF_CHECK_EQUAL(to_string(x), "[42]");
  x.convert_to_list();
  CAF_CHECK_EQUAL(to_string(x), "[42]");
}

CAF_TEST(append) {
  config_value x{int64_t{1}};
  CAF_CHECK_EQUAL(to_string(x), "1");
  x.append(config_value{int64_t{2}});
  CAF_CHECK_EQUAL(to_string(x), "[1, 2]");
  x.append(config_value{atom("foo")});
  CAF_CHECK_EQUAL(to_string(x), "[1, 2, 'foo']");
}

CAF_TEST(homogeneous dictionary) {
  using integer_map = std::map<std::string, int64_t>;
  auto xs = dict()
              .add("value-1", config_value{1})
              .add("value-2", config_value{2})
              .add("value-3", config_value{3})
              .add("value-4", config_value{4})
              .make();
  integer_map ys{
    {"value-1", 1},
    {"value-2", 2},
    {"value-3", 3},
    {"value-4", 4},
  };
  CAF_CHECK_EQUAL(get<int64_t>(xs, "value-1"), 1);
  CAF_CHECK_EQUAL(get<integer_map>(config_value{xs}), ys);
}

CAF_TEST(heterogeneous dictionary) {
  using string_list = std::vector<std::string>;
  auto xs = dict()
              .add("scheduler", dict()
                                  .add("policy", config_value{atom("none")})
                                  .add("max-threads", config_value{2})
                                  .make_cv())
              .add("nodes", dict()
                              .add("preload", cfg_lst("sun", "venus", "mercury",
                                                      "earth", "mars"))
                              .make_cv())

              .make();
  CAF_CHECK_EQUAL(get<atom_value>(xs, {"scheduler", "policy"}), atom("none"));
  CAF_CHECK_EQUAL(get<int64_t>(xs, {"scheduler", "max-threads"}), 2);
  CAF_CHECK_EQUAL(get_if<double>(&xs, {"scheduler", "max-threads"}), none);
  string_list nodes{"sun", "venus", "mercury", "earth", "mars"};
  CAF_CHECK_EQUAL(get<string_list>(xs, {"nodes", "preload"}), nodes);
}
