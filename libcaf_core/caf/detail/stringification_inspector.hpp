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
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/inspector_access.hpp"
#include "caf/save_inspector_base.hpp"
#include "caf/string_view.hpp"
#include "caf/timespan.hpp"
#include "caf/timestamp.hpp"
#include "caf/variant.hpp"

namespace caf::detail {

class CAF_CORE_EXPORT stringification_inspector
  : public save_inspector_base<stringification_inspector> {
public:
  // -- member types -----------------------------------------------------------

  using super = save_inspector_base<stringification_inspector>;

  // -- constructors, destructors, and assignment operators --------------------

  stringification_inspector(std::string& result) : result_(result) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  constexpr bool has_human_readable_format() const noexcept {
    return true;
  }

  bool always_quote_strings = true;

  // -- serializer interface ---------------------------------------------------

  bool begin_object(string_view name);

  bool end_object();

  bool begin_field(string_view);

  bool begin_field(string_view name, bool is_present);

  bool begin_field(string_view name, span<const type_id_t>, size_t);

  bool begin_field(string_view name, bool, span<const type_id_t>, size_t);

  bool end_field();

  bool begin_tuple(size_t size) {
    return begin_sequence(size);
  }

  bool end_tuple() {
    return end_sequence();
  }

  bool begin_key_value_pair() {
    return begin_tuple(2);
  }

  bool end_key_value_pair() {
    return end_tuple();
  }

  bool begin_sequence(size_t size);

  bool end_sequence();

  bool begin_associative_array(size_t size) {
    return begin_sequence(size);
  }

  bool end_associative_array() {
    return end_sequence();
  }

  bool value(byte x);

  bool value(bool x);

  template <class Integral>
  std::enable_if_t<std::is_integral<Integral>::value, bool> value(Integral x) {
    if constexpr (std::is_signed<Integral>::value)
      return int_value(static_cast<int64_t>(x));
    else
      return int_value(static_cast<uint64_t>(x));
  }

  bool value(float x);

  bool value(double x);

  bool value(long double x);

  bool value(timespan x);

  bool value(timestamp x);

  bool value(string_view x);

  bool value(const std::u16string& x);

  bool value(const std::u32string& x);

  bool value(span<const byte> x);

  using super::list;

  bool list(const std::vector<bool>& xs);

  // -- builtin inspection to pick up to_string or provide nicer formatting ----

  template <class Rep, class Period>
  bool builtin_inspect(const std::chrono::duration<Rep, Period> x) {
    return value(std::chrono::duration_cast<timespan>(x));
  }

  template <class T>
  std::enable_if_t<detail::is_map_like_v<T>, bool>
  builtin_inspect(const T& xs) {
    sep();
    auto i = xs.begin();
    auto last = xs.end();
    if (i == last) {
      result_ += "{}";
      return true;
    }
    result_ += '{';
    save(*this, i->first);
    result_ += " = ";
    save(*this, i->second);
    while (++i != last) {
      sep();
      save(*this, i->first);
      result_ += " = ";
      save(*this, i->second);
    }
    result_ += '}';
    return true;
  }

  template <class T>
  std::enable_if_t<has_to_string<T>::value
                     && !std::is_convertible<T, string_view>::value,
                   bool>
  builtin_inspect(const T& x) {
    auto str = to_string(x);
    if constexpr (std::is_convertible<decltype(str), const char*>::value) {
      const char* cstr = str;
      sep();
      result_ += cstr;
    } else {
      append(str);
    }
    return true;
  }

  template <class T>
  bool builtin_inspect(const T* x) {
    if (x == nullptr) {
      sep();
      result_ += "null";
      return true;
    } else if constexpr (std::is_same<T, char>::value) {
      return value(string_view{x, strlen(x)});
    } else {
      sep();
      result_ += '*';
      save(*this, detail::as_mutable_ref(*x));
      return true;
    }
  }

  template <class T>
  bool builtin_inspect(const optional<T>& x) {
    sep();
    if (!x) {
      result_ += "null";
    } else {
      result_ += '*';
      save(*this, detail::as_mutable_ref(*x));
    }
    return true;
  }

  // -- fallbacks --------------------------------------------------------------

  template <class T>
  bool opaque_value(T& val) {
    if constexpr (detail::is_iterable<T>::value) {
      print_list(val.begin(), val.end());
    } else {
      sep();
      result_ += "<unprintable>";
    }
    return true;
  }

private:
  template <class T>
  void append(T&& str) {
    sep();
    result_.insert(result_.end(), str.begin(), str.end());
  }

  template <class Iterator, class Sentinel>
  void print_list(Iterator first, Sentinel sentinel) {
    sep();
    result_ += '[';
    while (first != sentinel)
      save(*this, *first++);
    result_ += ']';
  }

  bool int_value(int64_t x);

  bool int_value(uint64_t x);

  void sep();

  std::string& result_;

  bool in_string_object_ = false;
};

} // namespace caf::detail
