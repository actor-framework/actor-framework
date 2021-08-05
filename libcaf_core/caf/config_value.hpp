// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <string_view>
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
#include "caf/raise_error.hpp"
#include "caf/string_algorithms.hpp"
#include "caf/sum_type.hpp"
#include "caf/sum_type_access.hpp"
#include "caf/sum_type_token.hpp"
#include "caf/timespan.hpp"
#include "caf/timestamp.hpp"
#include "caf/uri.hpp"
#include "caf/variant.hpp"

namespace caf::detail {

template <class T>
struct is_config_value_type : std::false_type {};

#define CAF_ADD_CONFIG_VALUE_TYPE(type_name)                                   \
  template <>                                                                  \
  struct is_config_value_type<type_name> : std::true_type {}

CAF_ADD_CONFIG_VALUE_TYPE(none_t);
CAF_ADD_CONFIG_VALUE_TYPE(int64_t);
CAF_ADD_CONFIG_VALUE_TYPE(bool);
CAF_ADD_CONFIG_VALUE_TYPE(double);
CAF_ADD_CONFIG_VALUE_TYPE(timespan);
CAF_ADD_CONFIG_VALUE_TYPE(uri);
CAF_ADD_CONFIG_VALUE_TYPE(std::string);
CAF_ADD_CONFIG_VALUE_TYPE(std::vector<config_value>);
CAF_ADD_CONFIG_VALUE_TYPE(dictionary<config_value>);

#undef CAF_ADD_CONFIG_VALUE_TYPE

template <class T>
constexpr bool is_config_value_type_v = is_config_value_type<T>::value;

} // namespace caf::detail

namespace caf {

/// A type for config parameters with similar interface to a `variant`. This
/// type is not implemented as a simple variant alias because variants cannot
/// contain lists of themselves.
class CAF_CORE_EXPORT config_value {
public:
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
  static expected<config_value> parse(std::string_view::iterator first,
                                      std::string_view::iterator last);

  /// Tries to parse a value from `str`.
  static expected<config_value> parse(std::string_view str);

  /// Tries to parse a config value (list) from `str` and to convert it to an
  /// allowed input message type for `Handle`.
  template <class Handle>
  static std::optional<message> parse_msg(std::string_view str, const Handle&) {
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

  /// @private
  ptrdiff_t signed_index() const noexcept;

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
  expected<uri> to_uri() const;

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

  template <class T>
  error assign(const T& x) {
    if constexpr (detail::is_config_value_type_v<T>) {
      data_ = x;
      return {};
    } else {
      config_value_writer writer{this};
      if (writer.apply(x))
        return {};
      else
        return {writer.move_error()};
    }
  }

  template <class T>
  static constexpr std::string_view mapped_type_name() {
    if constexpr (detail::is_complete<caf::type_name<T>>) {
      return caf::type_name<T>::value;
    } else if constexpr (detail::is_list_like_v<T>) {
      return "list";
    } else {
      return "dictionary";
    }
  }

private:
  // -- properties -------------------------------------------------------------

  static const char* type_name_at_index(size_t index) noexcept;

  static std::optional<message>
  parse_msg_impl(std::string_view str, span<const type_id_list> allowed_types);

  // -- auto conversion of related types ---------------------------------------

  template <class T>
  void set_range(T& xs, std::true_type) {
    auto& dict = as_dictionary();
    dict.clear();
    for (auto& [key, val] : xs)
      dict.emplace(key, std::move(val));
  }

  template <class T>
  void set_range(T& xs, std::false_type) {
    auto& ls = as_list();
    ls.clear();
    ls.insert(ls.end(), std::make_move_iterator(xs.begin()),
              std::make_move_iterator(xs.end()));
  }

  template <class T>
  void set(T x) {
    if constexpr (detail::is_config_value_type_v<T>) {
      data_ = std::move(x);
    } else if constexpr (std::is_integral<T>::value) {
      data_ = static_cast<int64_t>(x);
    } else if constexpr (std::is_convertible<T, const char*>::value) {
      data_ = std::string{x};
    } else {
      static_assert(detail::is_iterable<T>::value);
      using value_type = typename T::value_type;
      detail::bool_token<detail::is_pair<value_type>::value> is_map_type;
      set_range(x, is_map_type);
    }
  }

  void set(float x) {
    data_ = static_cast<double>(x);
  }

  void set(const char* x) {
    data_ = std::string{x};
  }

  void set(std::string_view x) {
    data_ = std::string{x.begin(), x.end()};
  }

  // -- member variables -------------------------------------------------------

  variant_type data_;
};

/// @relates config_value
CAF_CORE_EXPORT std::string to_string(const config_value& x);

// -- conversion via get_as ----------------------------------------------------

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
        return static_cast<T>(*result);
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
    if constexpr (detail::has_reserve_v<T>)
      result.reserve(wrapped_values->size());
    for (const auto& wrapped_value : *wrapped_values)
      if (auto maybe_value = get_as<value_type>(wrapped_value)) {
        if constexpr (detail::has_emplace_back_v<T>)
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
  } else if constexpr (std::is_same<T, uri>::value) {
    return value.to_uri();
  } else {
    auto token = inspect_access_type<config_value_reader, T>();
    return get_as<T>(value, token);
  }
}

// -- conversion via get_or ----------------------------------------------------

/// Customization point for configuring automatic mappings from default value
/// types to deduced types. For example, `get_or(value, "foo"sv)` must return a
/// `string` rather than a `string_view`. However, user-defined overloads
/// *must not* specialize this class for any type from the namespaces `std` or
/// `caf`.
template <class T>
struct get_or_deduction_guide {
  using value_type = T;
  template <class V>
  static decltype(auto) convert(V&& x) {
    return std::forward<V>(x);
  }
};

template <>
struct get_or_deduction_guide<std::string_view> {
  using value_type = std::string;
  static value_type convert(std::string_view str) {
    return {str.begin(), str.end()};
  }
};

template <>
struct get_or_deduction_guide<const char*> {
  using value_type = std::string;
  static value_type convert(const char* str) {
    return {str};
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
[[deprecated("use get_as or get_or instead")]] std::optional<T>
legacy_get_if(const config_value* x) {
  if (auto val = get_as<T>(*x))
    return {std::move(*val)};
  else
    return {};
}

template <class T>
auto get_if(const config_value* x) {
  if constexpr (detail::is_config_value_type_v<T>) {
    return get_if<T>(x->get_data_ptr());
  } else {
    return legacy_get_if<T>(x);
  }
}

template <class T>
auto get_if(config_value* x) {
  if constexpr (detail::is_config_value_type_v<T>) {
    return get_if<T>(x->get_data_ptr());
  } else {
    return legacy_get_if<T>(x);
  }
}

template <class T>
[[deprecated("use get_as or get_or instead")]] T
legacy_get(const config_value& x) {
  auto val = get_as<T>(x);
  if (!val)
    CAF_RAISE_ERROR("legacy_get: conversion failed");
  return std::move(*val);
}

template <class T>
decltype(auto) get(const config_value& x) {
  if constexpr (detail::is_config_value_type_v<T>) {
    return get<T>(x.get_data());
  } else {
    return legacy_get<T>(x);
  }
}

template <class T>
decltype(auto) get(config_value& x) {
  if constexpr (detail::is_config_value_type_v<T>) {
    return get<T>(x.get_data());
  } else {
    return legacy_get<T>(x);
  }
}

template <class T>
[[deprecated("use get_as or get_or instead")]] bool
legacy_holds_alternative(const config_value& x) {
  if (auto val = get_as<T>(x))
    return true;
  else
    return false;
}

template <class T>
auto holds_alternative(const config_value& x) {
  if constexpr (detail::is_config_value_type_v<T>) {
    return holds_alternative<T>(x.get_data());
  } else {
    return legacy_holds_alternative<T>(x);
  }
}

// -- comparison operator overloads --------------------------------------------

/// @relates config_value
CAF_CORE_EXPORT bool operator<(const config_value& x, const config_value& y);

/// @relates config_value
CAF_CORE_EXPORT bool operator<=(const config_value& x, const config_value& y);

/// @relates config_value
CAF_CORE_EXPORT bool operator==(const config_value& x, const config_value& y);

/// @relates config_value
CAF_CORE_EXPORT bool operator>(const config_value& x, const config_value& y);

/// @relates config_value
CAF_CORE_EXPORT bool operator>=(const config_value& x, const config_value& y);

/// @relates config_value
inline bool operator!=(const config_value& x, const config_value& y) {
  return !(x == y);
}

/// @relates config_value
CAF_CORE_EXPORT std::ostream& operator<<(std::ostream& out,
                                         const config_value& x);

// -- convenience APIs ---------------------------------------------------------

template <class... Ts>
config_value make_config_value_list(Ts&&... xs) {
  std::vector<config_value> lst{config_value{std::forward<Ts>(xs)}...};
  return config_value{std::move(lst)};
}

// -- inspection API -----------------------------------------------------------

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
    return caf::visit(std::forward<F>(f), x.get_data());
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
