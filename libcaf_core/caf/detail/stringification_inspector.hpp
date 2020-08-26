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
#include <string>
#include <type_traits>
#include <vector>

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/inspector_access.hpp"
#include "caf/save_inspector.hpp"
#include "caf/string_view.hpp"
#include "caf/timespan.hpp"
#include "caf/timestamp.hpp"

namespace caf::detail {

class CAF_CORE_EXPORT stringification_inspector : public save_inspector {
public:
  // -- member types -----------------------------------------------------------

  using super = save_inspector;

  // -- constructors, destructors, and assignment operators --------------------

  stringification_inspector(std::string& result) : result_(result) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  constexpr bool has_human_readable_format() const noexcept {
    return true;
  }

  // -- serializer interface ---------------------------------------------------

  bool begin_object(string_view name);

  bool end_object();

  bool begin_field(string_view);

  bool begin_field(string_view name, bool is_present);

  bool begin_field(string_view name, span<const type_id_t>, size_t);

  bool begin_field(string_view name, bool, span<const type_id_t>, size_t);

  bool end_field();

  bool begin_sequence(size_t size);

  bool end_sequence();

  bool begin_tuple(size_t size);

  bool end_tuple();

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

  bool value(const std::vector<bool>& xs);

  template <class T>
  std::enable_if_t<has_to_string<T>::value, bool> value(const T& x) {
    auto str = to_string(x);
    append(str);
    return true;
  }

  template <class T>
  bool value(const T* x) {
    sep();
    if (!x) {
      result_ += "null";
    } else {
      result_ += '*';
      inspect_value(*this, detail::as_mutable_ref(*x));
    }
    return true;
  }

  template <class T>
  void append(T&& str) {
    sep();
    result_.insert(result_.end(), str.begin(), str.end());
  }

  template <class T>
  bool opaque_value(const T& val) {
    sep();
    if constexpr (detail::is_iterable<T>::value) {
      result_ += '[';
      for (auto&& x : val)
        inspect_value(*this, detail::as_mutable_ref(x));
      result_ += ']';
    } else {
      result_ += "<unprintable>";
    }
    return true;
  }

  // -- DSL entry point --------------------------------------------------------

  template <class Object>
  constexpr auto object(Object&) noexcept {
    using wrapper_type = object_t<stringification_inspector>;
    return wrapper_type{type_name_or_anonymous<Object>(), this};
  }

private:
  bool int_value(int64_t x);

  bool int_value(uint64_t x);

  void sep();

  std::string& result_;
};

} // namespace caf::detail
