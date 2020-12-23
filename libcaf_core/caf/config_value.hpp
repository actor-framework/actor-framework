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

#pragma once

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <iterator>
#include <map>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#include "caf/config_value_reader.hpp"
#include "caf/config_value_writer.hpp"
#include "caf/detail/bounds_checker.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/move_if_not_ptr.hpp"
#include "caf/detail/parse.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/dictionary.hpp"
#include "caf/expected.hpp"
#include "caf/fwd.hpp"
#include "caf/inspector_access.hpp"
#include "caf/inspector_access_type.hpp"
#include "caf/optional.hpp"
#include "caf/raise_error.hpp"
#include "caf/string_algorithms.hpp"
#include "caf/string_view.hpp"
#include "caf/sum_type.hpp"
#include "caf/sum_type_access.hpp"
#include "caf/sum_type_token.hpp"
#include "caf/timespan.hpp"
#include "caf/timestamp.hpp"
#include "caf/uri.hpp"
#include "caf/variant.hpp"

namespace caf {

/// A type for config parameters with similar interface to a `variant`. This
/// type is not implemented as a simple variant alias because variants cannot
/// contain lists of themselves.
class CAF_CORE_EXPORT config_value {
public:
  // -- friends ----------------------------------------------------------------

  template <class T>
  friend expected<T> get_as(const config_value& value);

  // -- member types -----------------------------------------------------------

  using integer = int64_t;

  using boolean = bool;

  using real = double;

  using string = std::string;

  using list = std::vector<config_value>;

  using dictionary = caf::dictionary<config_value>;

  using types = detail::type_list<none_t, integer, boolean, real, timespan, uri,
                                  string, list, dictionary>;

  using variant_type = detail::tl_apply_t<types, variant>;

  // -- constructors, destructors, and assignment operators --------------------

  config_value() = default;

  config_value(config_value&& other) = default;

  config_value(const config_value& other) = default;

  template <class T, class E = detail::enable_if_t<
                       !std::is_same<detail::decay_t<T>, config_value>::value>>
  explicit config_value(T&& x) {
    set(std::forward<T>(x));
  }

  config_value& operator=(config_value&& other) = default;

  config_value& operator=(const config_value& other) = default;

  template <class T, class E = detail::enable_if_t<
                       !std::is_same<detail::decay_t<T>, config_value>::value>>
  config_value& operator=(T&& x) {
    set(std::forward<T>(x));
    return *this;
  }

  ~config_value();

  // -- parsing ----------------------------------------------------------------

  /// Tries to parse a value from given characters.
  static expected<config_value> parse(string_view::iterator first,
                                      string_view::iterator last);

  /// Tries to parse a value from `str`.
  static expected<config_value> parse(string_view str);

  /// Tries to parse a config value (list) from `str` and to convert it to an
  /// allowed input message type for `Handle`.
  template <class Handle>
  static optional<message> parse_msg(string_view str, const Handle&) {
    auto allowed = Handle::allowed_inputs();
    return parse_msg_impl(str, allowed);
  }

  // -- properties -------------------------------------------------------------

  /// Converts the value to a list with one element (unless the config value
  /// holds `nullptr`). Does nothing if the value already is a list.
  void convert_to_list();

  /// Returns the value as a list, converting it to one if needed.
  list& as_list();

  /// Returns the value as a dictionary, converting it to one if needed. The
  /// only data structure that CAF can convert to a dictionary is a list of
  /// lists, where each nested list contains exactly two elements (key and
  /// value). In all other cases, the conversion results in an empty dictionary.
  dictionary& as_dictionary();

  /// Appends `x` to a list. Converts this config value to a list first by
  /// calling `convert_to_list` if needed.
  void append(config_value x);

  /// Returns a human-readable type name of the current value.
  const char* type_name() const noexcept;

  /// Returns the underlying variant.
  variant_type& get_data() {
    return data_;
  }

  /// Returns the underlying variant.
  const variant_type& get_data() const {
    return data_;
  }

  /// Returns a pointer to the underlying variant.
  variant_type* get_data_ptr() {
    return &data_;
  }

  /// Returns a pointer to the underlying variant.
  const variant_type* get_data_ptr() const {
    return &data_;
  }

  /// Checks whether this config value is not null.
  explicit operator bool() const noexcept {
    return data_.index() != 0;
  }

  /// Checks whether this config value is null.
  bool operator!() const noexcept {
    return data_.index() == 0;
  }

  // -- utility ----------------------------------------------------------------

  /// @private
  type_id_t type_id() const noexcept;

  /// @private
  error_code<sec> default_construct(type_id_t);

  /// @private
  expected<bool> to_boolean() const;

  /// @private
  expected<integer> to_integer() const;

  /// @private
  expected<real> to_real() const;

  /// @private
  expected<timespan> to_timespan() const;

  /// @private
  expected<list> to_list() const;

  /// @private
  expected<dictionary> to_dictionary() const;

  /// @private
  bool can_convert_to_dictionary() const;

  /// @private
  template <class T, class Token>
  expected<T> convert_to(Token token) const {
    auto tmp = T{};
    config_value_reader reader{this};
    if (detail::load(reader, tmp, token))
      return {std::move(tmp)};
    else
      return {reader.move_error()};
  }

  /// @experimental
  template <class T>
  error assign(const T& x) {
    config_value_writer writer{this};
    if (writer.apply(x))
      return {};
    else
      return {writer.move_error()};
  }

private:
  // -- properties -------------------------------------------------------------

  static const char* type_name_at_index(size_t index) noexcept;

  static optional<message>
  parse_msg_impl(string_view str, span<const type_id_list> allowed_types);

  // -- auto conversion of related types ---------------------------------------

  void set(none_t) {
    data_ = none;
  }

  void set(bool x) {
    data_ = x;
  }

  void set(float x) {
    data_ = static_cast<double>(x);
  }

  void set(const char* x) {
    data_ = std::string{x};
  }

  void set(string_view x) {
    data_ = std::string{x.begin(), x.end()};
  }

  template <class T>
  detail::enable_if_t<
    detail::is_one_of<T, real, timespan, uri, string, list, dictionary>::value>
  set(T x) {
    data_ = std::move(x);
  }

  template <class T>
  void set_range(T& xs, std::true_type) {
    auto& dict = as_dictionary();
    dict.clear();
    for (auto& kvp : xs)
      dict.emplace(kvp.first, std::move(kvp.second));
  }

  template <class T>
  void set_range(T& xs, std::false_type) {
    auto& ls = as_list();
    ls.clear();
    ls.insert(ls.end(), std::make_move_iterator(xs.begin()),
              std::make_move_iterator(xs.end()));
  }

  template <class T>
  detail::enable_if_t<detail::is_iterable<T>::value
                      && !detail::is_one_of<T, string, list, dictionary>::value>
  set(T xs) {
    using value_type = typename T::value_type;
    detail::bool_token<detail::is_pair<value_type>::value> is_map_type;
    set_range(xs, is_map_type);
  }

  template <class T>
  detail::enable_if_t<std::is_integral<T>::value> set(T x) {
    data_ = static_cast<int64_t>(x);
  }

  // -- member variables -------------------------------------------------------

  variant_type data_;
};

/// @relates config_value
CAF_CORE_EXPORT std::string to_string(const config_value& x);

// -- conversion via get_as ----------------------------------------------------

template <class T>
expected<T> get_as(const config_value& value);

template <class T>
expected<T> get_as(const config_value&, inspector_access_type::none) {
  static_assert(detail::always_false_v<T>,
                "cannot convert to T: found no a suitable inspect overload");
}

template <class T>
expected<T> get_as(const config_value&, inspector_access_type::unsafe) {
  static_assert(detail::always_false_v<T>,
                "cannot convert types that are tagged as unsafe");
}

template <class T>
expected<T>
get_as(const config_value& x, inspector_access_type::specialization token) {
  return x.convert_to<T>(token);
}

template <class T>
expected<T>
get_as(const config_value& x, inspector_access_type::inspect token) {
  return x.convert_to<T>(token);
}

template <class T>
expected<T>
get_as(const config_value& x, inspector_access_type::builtin_inspect token) {
  return x.convert_to<T>(token);
}

template <class T>
expected<T> get_as(const config_value& x, inspector_access_type::builtin) {
  if constexpr (std::is_same<T, std::string>::value) {
    return to_string(x);
  } else if constexpr (std::is_same<T, bool>::value) {
    return x.to_boolean();
  } else if constexpr (std::is_integral<T>::value) {
    if (auto result = x.to_integer()) {
      if (detail::bounds_checker<T>::check(*result))
        return *result;
      else
        return make_error(sec::conversion_failed, "narrowing error");
    } else {
      return std::move(result.error());
    }
  } else if constexpr (std::is_floating_point<T>::value) {
    if (auto result = x.to_real()) {
      if constexpr (sizeof(T) >= sizeof(config_value::real)) {
        return *result;
      } else {
        auto narrowed = static_cast<T>(*result);
        if (!std::isfinite(*result) || std::isfinite(narrowed)) {
          return narrowed;
        } else {
          return make_error(sec::conversion_failed, "narrowing error");
        }
      }
    } else {
      return std::move(result.error());
    }
  } else {
    static_assert(detail::always_false_v<T>,
                  "sorry, this conversion is not implemented yet");
  }
}

template <class T>
expected<T> get_as(const config_value& x, inspector_access_type::empty) {
  // Technically, we could always simply return T{} here. However,
  // *semantically* it only makes sense to converts dictionaries to objects. So
  // at least we check for this condition here.
  if (x.can_convert_to_dictionary())
    return T{};
  else
    return make_error(sec::conversion_failed,
                      "invalid element type: expected a dictionary");
}

template <class T, size_t... Is>
expected<T>
get_as_tuple(const config_value::list& x, std::index_sequence<Is...>) {
  auto boxed = std::make_tuple(get_as<std::tuple_element_t<Is, T>>(x[Is])...);
  if ((get<Is>(boxed) && ...))
    return T{std::move(*get<Is>(boxed))...};
  else
    return make_error(sec::conversion_failed, "invalid element types");
}

template <class T>
expected<T> get_as(const config_value& x, inspector_access_type::tuple) {
  static_assert(!std::is_array<T>::value,
                "cannot return an array from a function");
  if (auto wrapped_values = x.to_list()) {
    static constexpr size_t n = std::tuple_size<T>::value;
    if (wrapped_values->size() == n)
      return get_as_tuple<T>(*wrapped_values, std::make_index_sequence<n>{});
    else
      return make_error(sec::conversion_failed, "wrong number of arguments");
  } else {
    return {std::move(wrapped_values.error())};
  }
}

template <class T>
expected<T> get_as(const config_value& x, inspector_access_type::map) {
  using key_type = typename T::key_type;
  using mapped_type = typename T::mapped_type;
  T result;
  if (auto dict = x.to_dictionary()) {
    for (auto&& [string_key, wrapped_value] : *dict) {
      config_value wrapped_key{std::move(string_key)};
      if (auto key = get_as<key_type>(wrapped_key)) {
        if (auto val = get_as<mapped_type>(wrapped_value)) {
          if (!result.emplace(std::move(*key), std::move(*val)).second) {
            return make_error(sec::conversion_failed,
                              "ambiguous mapping of keys to key_type");
          }
        } else {
          return make_error(sec::conversion_failed,
                            "failed to convert values to mapped_type");
        }
      } else {
        return make_error(sec::conversion_failed,
                          "failed to convert keys to key_type");
      }
    }
    return {std::move(result)};
  } else {
    return {std::move(dict.error())};
  }
}

template <class T>
expected<T> get_as(const config_value& x, inspector_access_type::list) {
  if (auto wrapped_values = x.to_list()) {
    using value_type = typename T::value_type;
    T result;
    if constexpr (detail::has_reserve_t<T>)
      result.reserve(wrapped_values->size());
    for (const auto& wrapped_value : *wrapped_values)
      if (auto maybe_value = get_as<value_type>(wrapped_value)) {
        if constexpr (detail::has_emplace_back_t<T>)
          result.emplace_back(std::move(*maybe_value));
        else
          result.insert(result.end(), std::move(*maybe_value));
      } else {
        return {std::move(maybe_value.error())};
      }
    return {std::move(result)};
  } else {
    return {std::move(wrapped_values.error())};
  }
}

/// Converts a @ref config_value to builtin types or user-defined types that
/// opted into the type inspection API.
/// @relates config_value
template <class T>
expected<T> get_as(const config_value& value) {
  if constexpr (std::is_same<T, timespan>::value) {
    return value.to_timespan();
  } else if constexpr (std::is_same<T, config_value::list>::value) {
    return value.to_list();
  } else if constexpr (std::is_same<T, config_value::dictionary>::value) {
    return value.to_dictionary();
  } else {
    auto token = inspect_access_type<config_value_reader, T>();
    return get_as<T>(value, token);
  }
}

// -- conversion via get_or ----------------------------------------------------

/// Customization point for configuring automatic mappings from default value
/// types to deduced types. For example, `get_or(value, "foo"sv)` must return a
/// `string` rather than a `string_view`. However, user-defined overloads *must
/// not* specialize this class for any type from the namespaces `std` or `caf`.
template <class T>
struct get_or_deduction_guide {
  using value_type = T;
  template <class V>
  static decltype(auto) convert(V&& x) {
    return std::forward<V>(x);
  }
};

template <>
struct get_or_deduction_guide<string_view> {
  using value_type = std::string;
  static value_type convert(string_view str) {
    return {str.begin(), str.end()};
  }
};

template <class T>
struct get_or_deduction_guide<span<T>> {
  using value_type = std::vector<T>;
  static value_type convert(span<T> buf) {
    return {buf.begin(), buf.end()};
  }
};

/// Configures @ref get_or to uses the @ref get_or_deduction_guide.
struct get_or_auto_deduce {};

/// Converts a @ref config_value to `To` or returns `fallback` if the conversion
/// fails.
/// @relates config_value
template <class To = get_or_auto_deduce, class Fallback>
auto get_or(const config_value& x, Fallback&& fallback) {
  if constexpr (std::is_same<To, get_or_auto_deduce>::value) {
    using guide = get_or_deduction_guide<std::decay_t<Fallback>>;
    using value_type = typename guide::value_type;
    if (auto val = get_as<value_type>(x))
      return std::move(*val);
    else
      return guide::convert(std::forward<Fallback>(fallback));
  } else {
    if (auto val = get_as<To>(x))
      return std::move(*val);
    else
      return To{std::forward<Fallback>(fallback)};
  }
}

// -- SumType-like access ------------------------------------------------------

template <class T>
struct default_config_value_access {
  static bool is(const config_value& x) {
    return holds_alternative<T>(x.get_data());
  }

  static const T* get_if(const config_value* x) {
    return caf::get_if<T>(&(x->get_data()));
  }

  static T get(const config_value& x) {
    return caf::get<T>(x.get_data());
  }

  static T convert(T x) {
    return x;
  }
};

/// @relates config_value
template <class T>
struct config_value_access;

#define CAF_DEFAULT_CONFIG_VALUE_ACCESS(type, name)                            \
  template <>                                                                  \
  struct CAF_CORE_EXPORT config_value_access<type>                             \
    : default_config_value_access<type> {                                      \
    static std::string type_name() {                                           \
      return name;                                                             \
    }                                                                          \
  }

CAF_DEFAULT_CONFIG_VALUE_ACCESS(bool, "boolean");
CAF_DEFAULT_CONFIG_VALUE_ACCESS(double, "real64");
CAF_DEFAULT_CONFIG_VALUE_ACCESS(uri, "uri");

#undef CAF_DEFAULT_CONFIG_VALUE_ACCESS

template <>
struct CAF_CORE_EXPORT config_value_access<timespan> {
  static std::string type_name() {
    return "timespan";
  }

  static bool is(const config_value& x) {
    return static_cast<bool>(get_if(&x));
  }

  static optional<timespan> get_if(const config_value* x) {
    auto data_ptr = std::addressof(x->get_data());
    if (auto res = caf::get_if<timespan>(data_ptr))
      return static_cast<timespan>(*res);
    if (auto str = caf::get_if<std::string>(data_ptr)) {
      string_view sv{*str};
      timespan result;
      string_parser_state ps{sv.begin(), sv.end()};
      detail::parse(ps, result);
      if (ps.code == pec::success)
        return result;
    }
    return none;
  }

  static timespan get(const config_value& x) {
    auto result = get_if(&x);
    CAF_ASSERT(result);
    return *result;
  }

  static timespan convert(timespan x) {
    return x;
  }
};

template <>
struct CAF_CORE_EXPORT config_value_access<float> {
  static std::string type_name() {
    return "real32";
  }

  static bool is(const config_value& x) {
    return holds_alternative<double>(x.get_data());
  }

  static optional<float> get_if(const config_value* x) {
    if (auto res = caf::get_if<double>(&(x->get_data())))
      return static_cast<float>(*res);
    return none;
  }

  static float get(const config_value& x) {
    return static_cast<float>(caf::get<double>(x.get_data()));
  }

  static double convert(float x) {
    return x;
  }
};

template <>
struct CAF_CORE_EXPORT config_value_access<std::string> {
  using super = default_config_value_access<std::string>;

  static std::string type_name() {
    return "string";
  }

  static bool is(const config_value& x) {
    return holds_alternative<std::string>(x.get_data());
  }

  static const std::string* get_if(const config_value* x) {
    return caf::get_if<std::string>(&(x->get_data()));
  }

  static std::string get(const config_value& x) {
    return caf::get<std::string>(x.get_data());
  }

  static std::string convert(std::string x) {
    return x;
  }
};

// -- implementation details for get/get_if/holds_alternative ------------------

namespace detail {

/// Wraps tag types for static dispatching.
/// @relates config_value_access_type
struct config_value_access_type {
  /// Flags types that provide a `config_value_access` specialization.
  struct specialization {};

  /// Flags builtin integral types.
  struct integral {};

  /// Flags types with `std::tuple`-like API.
  struct tuple {};

  /// Flags types with `std::map`-like API.
  struct map {};

  /// Flags types with `std::vector`-like API.
  struct list {};

  /// Flags types without default access that shall fall back to using the
  /// inspection API.
  struct inspect {};
};

/// @relates config_value_access_type
template <class T>
constexpr auto config_value_access_token() {
  if constexpr (detail::is_complete<config_value_access<T>>)
    return config_value_access_type::specialization{};
  else if constexpr (std::is_integral<T>::value)
    return config_value_access_type::integral{};
  else if constexpr (detail::is_stl_tuple_type_v<T>)
    return config_value_access_type::tuple{};
  else if constexpr (detail::is_map_like<T>::value)
    return config_value_access_type::map{};
  else if constexpr (detail::is_list_like<T>::value)
    return config_value_access_type::list{};
  else
    return config_value_access_type::inspect{};
}

template <class T>
struct integral_config_value_access;

template <class T>
struct tuple_config_value_access;

template <class T>
struct map_config_value_access;

template <class T>
struct list_config_value_access;

template <class T>
struct inspect_config_value_access;

template <class T, class Token>
struct config_value_access_oracle;

template <class T>
struct config_value_access_oracle<T, config_value_access_type::specialization> {
  using type = config_value_access<T>;
};

#define CAF_CONFIG_VALUE_ACCESS_ORACLE(access_type)                            \
  template <class T>                                                           \
  struct config_value_access_oracle<T,                                         \
                                    config_value_access_type::access_type> {   \
    using type = access_type##_config_value_access<T>;                         \
  }

CAF_CONFIG_VALUE_ACCESS_ORACLE(integral);
CAF_CONFIG_VALUE_ACCESS_ORACLE(tuple);
CAF_CONFIG_VALUE_ACCESS_ORACLE(map);
CAF_CONFIG_VALUE_ACCESS_ORACLE(list);
CAF_CONFIG_VALUE_ACCESS_ORACLE(inspect);

#undef CAF_CONFIG_VALUE_ACCESS_ORACLE

template <class T>
using config_value_access_t = typename config_value_access_oracle<
  T, decltype(config_value_access_token<T>())>::type;

template <class T>
struct integral_config_value_access {
  using integer_type = config_value::integer;

  static std::string type_name() {
    std::string result;
    if (std::is_signed<T>::value)
      result = "int";
    else
      result = "uint";
    result += std::to_string(sizeof(T) * 8);
    return result;
  }

  static bool is(const config_value& x) {
    auto ptr = caf::get_if<integer_type>(x.get_data_ptr());
    return ptr != nullptr && detail::bounds_checker<T>::check(*ptr);
  }

  static optional<T> get_if(const config_value* x) {
    auto ptr = caf::get_if<integer_type>(x->get_data_ptr());
    if (ptr != nullptr && detail::bounds_checker<T>::check(*ptr))
      return static_cast<T>(*ptr);
    return none;
  }

  static T get(const config_value& x) {
    auto res = get_if(&x);
    CAF_ASSERT(res != none);
    return *res;
  }

  static T convert(T x) {
    return x;
  }
};

template <class T>
struct list_config_value_access {
  using list_type = T;

  using value_type = typename list_type::value_type;

  using value_access = config_value_access_t<value_type>;

  static std::string type_name() {
    return "list of " + value_access::type_name();
  }

  static bool is(const config_value& x) {
    auto lst = caf::get_if<config_value::list>(&(x.get_data()));
    return lst != nullptr
           && std::all_of(lst->begin(), lst->end(), [](const config_value& y) {
                return caf::holds_alternative<value_type>(y);
              });
    return false;
  }

  static optional<list_type> get_if(const config_value* x) {
    list_type result;
    auto out = std::inserter(result, result.end());
    auto extract = [&](const config_value& y) {
      if (auto opt = caf::get_if<value_type>(&y)) {
        *out++ = move_if_optional(opt);
        return true;
      }
      return false;
    };
    auto lst = caf::get_if<config_value::list>(&(x->get_data()));
    if (lst != nullptr && std::all_of(lst->begin(), lst->end(), extract))
      return result;
    return none;
  }

  static list_type get(const config_value& x) {
    auto result = get_if(&x);
    if (!result)
      CAF_RAISE_ERROR("invalid type found");
    return std::move(*result);
  }

  static config_value::list convert(const list_type& xs) {
    config_value::list result;
    for (const auto& x : xs)
      result.emplace_back(value_access::convert(x));
    return result;
  }
};

template <class T>
struct map_config_value_access {
  using map_type = T;

  using mapped_type = typename map_type::mapped_type;

  using mapped_access = config_value_access_t<mapped_type>;

  static std::string type_name() {
    std::string result = "dictionary of ";
    auto nested_name = mapped_access::type_name();
    result.insert(result.end(), nested_name.begin(), nested_name.end());
    return result;
  }

  static bool is(const config_value& x) {
    using value_type = config_value::dictionary::value_type;
    auto is_mapped_type = [](const value_type& y) {
      return caf::holds_alternative<mapped_type>(y.second);
    };
    if (auto dict = caf::get_if<config_value::dictionary>(&(x.get_data())))
      return std::all_of(dict->begin(), dict->end(), is_mapped_type);
    return false;
  }

  static optional<map_type> get_if(const config_value* x) {
    using value_type = config_value::dictionary::value_type;
    map_type result;
    auto extract = [&](const value_type& y) {
      if (auto opt = caf::get_if<mapped_type>(&y.second)) {
        result.emplace(y.first, move_if_optional(opt));
        return true;
      }
      return false;
    };
    if (auto dict = caf::get_if<config_value::dictionary>(&(x->get_data())))
      if (std::all_of(dict->begin(), dict->end(), extract))
        return result;
    return none;
  }

  static map_type get(const config_value& x) {
    auto result = get_if(&x);
    if (!result)
      CAF_RAISE_ERROR("invalid type found");
    return std::move(*result);
  }

  static config_value::dictionary convert(const map_type& xs) {
    config_value::dictionary result;
    for (const auto& x : xs)
      result.emplace(x.first, mapped_access::convert(x.second));
    return result;
  }
};

template <class... Ts>
struct tuple_config_value_access<std::tuple<Ts...>> {
  using tuple_type = std::tuple<Ts...>;

  static std::string type_name() {
    auto result = "tuple[";
    rec_name(result, true, detail::int_token<0>(), detail::type_list<Ts...>());
    result += ']';
    return result;
  }

  static bool is(const config_value& x) {
    if (auto lst = caf::get_if<config_value::list>(&(x.get_data()))) {
      if (lst->size() != sizeof...(Ts))
        return false;
      return rec_is(*lst, detail::int_token<0>(), detail::type_list<Ts...>());
    }
    return false;
  }

  static optional<tuple_type> get_if(const config_value* x) {
    if (auto lst = caf::get_if<config_value::list>(&(x->get_data()))) {
      if (lst->size() != sizeof...(Ts))
        return none;
      tuple_type result;
      if (rec_get(*lst, result, detail::int_token<0>(),
                  detail::type_list<Ts...>()))
        return result;
    }
    return none;
  }

  static tuple_type get(const config_value& x) {
    if (auto result = get_if(&x))
      return std::move(*result);
    CAF_RAISE_ERROR("invalid type found");
  }

  static config_value::list convert(const tuple_type& xs) {
    config_value::list result;
    rec_convert(result, xs, detail::int_token<0>(), detail::type_list<Ts...>());
    return result;
  }

private:
  template <int Pos>
  static void
  rec_name(std::string&, bool, detail::int_token<Pos>, detail::type_list<>) {
    // nop
  }

  template <int Pos, class U, class... Us>
  static void rec_name(std::string& result, bool is_first,
                       detail::int_token<Pos>, detail::type_list<U, Us...>) {
    if (!is_first)
      result += ", ";
    using nested = config_value_access<U>;
    auto nested_name = nested::type_name();
    result.insert(result.end(), nested_name.begin(), nested_name.end());
    return rec_name(result, false, detail::int_token<Pos + 1>(),
                    detail::type_list<Us...>());
  }

  template <int Pos>
  static bool rec_is(const config_value::list&, detail::int_token<Pos>,
                     detail::type_list<>) {
    return true;
  }

  template <int Pos, class U, class... Us>
  static bool rec_is(const config_value::list& xs, detail::int_token<Pos>,
                     detail::type_list<U, Us...>) {
    if (!holds_alternative<U>(xs[Pos]))
      return false;
    return rec_is(xs, detail::int_token<Pos + 1>(), detail::type_list<Us...>());
  }

  template <int Pos>
  static bool rec_get(const config_value::list&, tuple_type&,
                      detail::int_token<Pos>, detail::type_list<>) {
    return true;
  }

  template <int Pos, class U, class... Us>
  static bool rec_get(const config_value::list& xs, tuple_type& result,
                      detail::int_token<Pos>, detail::type_list<U, Us...>) {
    if (auto value = caf::get_if<U>(&xs[Pos])) {
      std::get<Pos>(result) = detail::move_if_not_ptr(value);
      return rec_get(xs, result, detail::int_token<Pos + 1>(),
                     detail::type_list<Us...>());
    }
    return false;
  }

  template <int Pos>
  static void rec_convert(config_value::list&, const tuple_type&,
                          detail::int_token<Pos>, detail::type_list<>) {
    // nop
  }

  template <int Pos, class U, class... Us>
  static void rec_convert(config_value::list& result, const tuple_type& xs,
                          detail::int_token<Pos>, detail::type_list<U, Us...>) {
    using trait = config_value_access_t<U>;
    result.emplace_back(trait::convert(std::get<Pos>(xs)));
    return rec_convert(result, xs, detail::int_token<Pos + 1>(),
                       detail::type_list<Us...>());
  }
};

} // namespace detail

// -- SumType access of dictionary values --------------------------------------

template <>
struct sum_type_access<config_value> {
  using types = typename config_value::types;

  using type0 = typename detail::tl_head<types>::type;

  static constexpr bool specialized = true;

  template <class U, int Pos>
  static bool is(const config_value& x, sum_type_token<U, Pos> token) {
    return x.get_data().is(pos(token));
  }

  template <class U>
  static bool is(const config_value& x, sum_type_token<U, -1>) {
    return detail::config_value_access_t<U>::is(x);
  }

  template <class U, int Pos>
  static U& get(config_value& x, sum_type_token<U, Pos> token) {
    return x.get_data().get(pos(token));
  }

  template <class U>
  static U get(config_value& x, sum_type_token<U, -1>) {
    return detail::config_value_access_t<U>::get(x);
  }

  template <class U, int Pos>
  static const U& get(const config_value& x, sum_type_token<U, Pos> token) {
    return x.get_data().get(pos(token));
  }

  template <class U>
  static U get(const config_value& x, sum_type_token<U, -1>) {
    return detail::config_value_access_t<U>::get(x);
  }

  template <class U, int Pos>
  static U* get_if(config_value* x, sum_type_token<U, Pos> token) {
    return is(*x, token) ? &get(*x, token) : nullptr;
  }

  template <class U>
  static optional<U> get_if(config_value* x, sum_type_token<U, -1>) {
    return detail::config_value_access_t<U>::get_if(x);
  }

  template <class U, int Pos>
  static const U* get_if(const config_value* x, sum_type_token<U, Pos> token) {
    return is(*x, token) ? &get(*x, token) : nullptr;
  }

  template <class U>
  static optional<U> get_if(const config_value* x, sum_type_token<U, -1>) {
    return detail::config_value_access_t<U>::get_if(x);
  }

  template <class Result, class Visitor, class... Ts>
  static Result apply(config_value& x, Visitor&& visitor, Ts&&... xs) {
    return x.get_data().template apply<Result>(std::forward<Visitor>(visitor),
                                               std::forward<Ts>(xs)...);
  }

  template <class Result, class Visitor, class... Ts>
  static Result apply(const config_value& x, Visitor&& visitor, Ts&&... xs) {
    return x.get_data().template apply<Result>(std::forward<Visitor>(visitor),
                                               std::forward<Ts>(xs)...);
  }
};

/// @relates config_value
inline bool operator==(const config_value& x, std::nullptr_t) noexcept {
  return x.get_data().index() == 0;
}

/// @relates config_value
inline bool operator==(std::nullptr_t, const config_value& x) noexcept {
  return x.get_data().index() == 0;
}

/// @relates config_value
inline bool operator!=(const config_value& x, std::nullptr_t) noexcept {
  return x.get_data().index() != 0;
}

/// @relates config_value
inline bool operator!=(std::nullptr_t, const config_value& x) noexcept {
  return x.get_data().index() != 0;
}

/// @relates config_value
CAF_CORE_EXPORT bool operator<(const config_value& x, const config_value& y);

/// @relates config_value
CAF_CORE_EXPORT bool operator==(const config_value& x, const config_value& y);

/// @relates config_value
inline bool operator>=(const config_value& x, const config_value& y) {
  return !(x < y);
}

/// @relates config_value
inline bool operator!=(const config_value& x, const config_value& y) {
  return !(x == y);
}

/// @relates config_value
CAF_CORE_EXPORT std::ostream& operator<<(std::ostream& out,
                                         const config_value& x);

template <class... Ts>
config_value make_config_value_list(Ts&&... xs) {
  std::vector<config_value> lst{config_value{std::forward<Ts>(xs)}...};
  return config_value{std::move(lst)};
}

// -- inspection API -----------------------------------------------------------

namespace detail {

template <class T>
struct inspect_config_value_access {
  static std::string type_name() {
    return to_string(type_name_or_anonymous<T>());
  }

  static optional<T> get_if(const config_value* x) {
    config_value_reader reader{x};
    auto tmp = T{};
    if (detail::load(reader, tmp))
      return optional<T>{std::move(tmp)};
    return none;
  }

  static bool is(const config_value& x) {
    return get_if(&x) != none;
  }

  static T get(const config_value& x) {
    auto result = get_if(&x);
    if (!result)
      CAF_RAISE_ERROR("invalid type found");
    return std::move(*result);
  }

  static config_value convert(const T& x) {
    config_value result;
    config_value_writer writer{&result};
    if (!detail::save(writer, x))
      CAF_RAISE_ERROR("unable to convert type to a config_value");
    return result;
  }
};

} // namespace detail

template <>
struct variant_inspector_traits<config_value> {
  using value_type = config_value;

  static constexpr type_id_t allowed_types[] = {
    type_id_v<none_t>,
    type_id_v<config_value::integer>,
    type_id_v<config_value::boolean>,
    type_id_v<config_value::real>,
    type_id_v<timespan>,
    type_id_v<uri>,
    type_id_v<config_value::string>,
    type_id_v<config_value::list>,
    type_id_v<config_value::dictionary>,
  };

  static auto type_index(const config_value& x) {
    return x.get_data().index();
  }

  template <class F, class Value>
  static auto visit(F&& f, Value&& x) {
    return caf::visit(std::forward<F>(f), std::forward<Value>(x));
  }

  template <class U>
  static void assign(value_type& x, U&& value) {
    x = std::move(value);
  }

  template <class F>
  static bool load(type_id_t type, F continuation) {
    switch (type) {
      default:
        return false;
      case type_id_v<none_t>: {
        auto tmp = config_value{};
        continuation(tmp);
        return true;
      }
      case type_id_v<config_value::integer>: {
        auto tmp = config_value::integer{};
        continuation(tmp);
        return true;
      }
      case type_id_v<config_value::boolean>: {
        auto tmp = config_value::boolean{};
        continuation(tmp);
        return true;
      }
      case type_id_v<config_value::real>: {
        auto tmp = config_value::real{};
        continuation(tmp);
        return true;
      }
      case type_id_v<timespan>: {
        auto tmp = timespan{};
        continuation(tmp);
        return true;
      }
      case type_id_v<uri>: {
        auto tmp = uri{};
        continuation(tmp);
        return true;
      }
      case type_id_v<config_value::string>: {
        auto tmp = config_value::string{};
        continuation(tmp);
        return true;
      }
      case type_id_v<config_value::list>: {
        auto tmp = config_value::list{};
        continuation(tmp);
        return true;
      }
      case type_id_v<config_value::dictionary>: {
        auto tmp = config_value::dictionary{};
        continuation(tmp);
        return true;
      }
    }
  }
};

template <>
struct inspector_access<config_value> : variant_inspector_access<config_value> {
  // nop
};

} // namespace caf
