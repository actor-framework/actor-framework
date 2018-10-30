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

#include "caf/atom.hpp"
#include "caf/error.hpp"
#include "caf/none.hpp"
#include "caf/string_view.hpp"
#include "caf/timespan.hpp"
#include "caf/timestamp.hpp"

#include "caf/meta/type_name.hpp"
#include "caf/meta/omittable.hpp"
#include "caf/meta/annotation.hpp"
#include "caf/meta/hex_formatted.hpp"
#include "caf/meta/omittable_if_none.hpp"
#include "caf/meta/omittable_if_empty.hpp"

#include "caf/detail/append_hex.hpp"
#include "caf/detail/apply_args.hpp"
#include "caf/detail/int_list.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf {
namespace detail {

class stringification_inspector {
public:
  // -- member types required by Inspector concept -----------------------------

  using result_type = void;

  static constexpr bool reads_state = true;
  static constexpr bool writes_state = false;

  // -- constructors, destructors, and assignment operators --------------------

  stringification_inspector(std::string& result) : result_(result) {
    // nop
  }

  // -- operator() -------------------------------------------------------------

  template <class... Ts>
  void operator()(Ts&&... xs) {
    traverse(xs...);
  }

  /// Prints a separator to the result string.
  void sep();

  inline void traverse() noexcept {
    // end of recursion
  }

  void consume(atom_value& x);

  void consume(string_view str);

  void consume(timespan& x);

  void consume(timestamp& x);

  template <class Clock, class Duration>
  void consume(std::chrono::time_point<Clock, Duration>& x) {
    timestamp tmp{std::chrono::duration_cast<timespan>(x.time_since_epoch())};
    consume(tmp);
  }

  template <class Rep, class Period>
  void consume(std::chrono::duration<Rep, Period>& x) {
    auto tmp = std::chrono::duration_cast<timespan>(x);
    consume(tmp);
  }

  inline void consume(bool& x) {
    result_ += x ? "true" : "false";
  }

  inline void consume(const char* cstr) {
    if (cstr == nullptr) {
      result_ += "null";
    } else {
      string_view tmp{cstr, strlen(cstr)};
      consume(tmp);
    }
  }

  inline void consume(char* cstr) {
    consume(const_cast<const char*>(cstr));
  }

  template <class T>
  enable_if_tt<std::is_arithmetic<T>> consume(T& x) {
    result_ += std::to_string(x);
  }

  // unwrap std::ref
  template <class T>
  void consume(std::reference_wrapper<T>& x) {
    return consume(x.get());
  }

  /// Picks up user-defined `to_string` functions.
  template <class T>
  enable_if_t<!std::is_pointer<T>::value && has_to_string<T>::value>
  consume(T& x) {
    result_ += to_string(x);
  }

  /// Delegates to `inspect(*this, x)` if available and `T`
  /// does not provide a `to_string`.
  template <class T>
  enable_if_t<
    is_inspectable<stringification_inspector, T>::value
    && !has_to_string<T>::value>
  consume(T& x) {
    inspect(*this, x);
  }

  template <class F, class S>
  void consume(std::pair<F, S>& x) {
    result_ += '(';
    traverse(deconst(x.first), deconst(x.second));
    result_ += ')';
  }

  template <class... Ts>
  void consume(std::tuple<Ts...>& x) {
    result_ += '(';
    apply_args(*this, get_indices(x), x);
    result_ += ')';
  }

  template <class T>
  enable_if_t<is_iterable<T>::value
              && !is_inspectable<stringification_inspector, T>::value
              && !std::is_convertible<T, string_view>::value
              && !has_to_string<T>::value>
  consume(T& xs) {
    result_ += '[';
    // use a hand-written for loop instead of for-each to avoid
    // range-loop-analysis warnings when using this function with vector<bool>
    for (auto i = xs.begin(); i != xs.end(); ++i) {
      sep();
      consume(deconst(*i));
    }
    result_ += ']';
  }

  template <class T>
  enable_if_t<has_peek_all<T>::value
              && !is_iterable<T>::value // pick begin()/end() over peek_all
              && !is_inspectable<stringification_inspector, T>::value
              && !has_to_string<T>::value>
  consume(T& xs) {
    result_ += '[';
    xs.peek_all(*this);
    result_ += ']';
  }

  template <class T>
  void consume(T* xs, size_t n) {
    result_ += '(';
    for (size_t i = 0; i < n; ++i) {
      sep();
      consume(deconst(xs[i]));
    }
    result_ += ')';
  }

  template <class T, size_t S>
  void consume(std::array<T, S>& xs) {
    return consume(xs.data(), S);
  }

  template <class T, size_t S>
  void consume(T (&xs)[S]) {
    return consume(xs, S);
  }

  template <class T>
  enable_if_t<!std::is_same<decay_t<T>, void>::value>
  consume(T*& ptr) {
    if (ptr) {
      result_ += '*';
      consume(*ptr);
    } else {
      result_ += "<null>";
    }
  }

  inline void consume(const void* ptr) {
    result_ += "0x";
    auto int_val = reinterpret_cast<intptr_t>(ptr);
    consume(int_val);
  }

  /// Print duration types with nanosecond resolution.
  template <class Rep>
  void consume(std::chrono::duration<Rep, std::nano>& x) {
    result_ += std::to_string(x.count());
    result_ += "ns";
  }

  /// Print duration types with microsecond resolution.
  template <class Rep>
  void consume(std::chrono::duration<Rep, std::micro>& x) {
    result_ += std::to_string(x.count());
    result_ += "us";
  }

  /// Print duration types with millisecond resolution.
  template <class Rep>
  void consume(std::chrono::duration<Rep, std::milli>& x) {
    result_ += std::to_string(x.count());
    result_ += "ms";
  }

  /// Print duration types with second resolution.
  template <class Rep>
  void consume(std::chrono::duration<Rep, std::ratio<1>>& x) {
    result_ += std::to_string(x.count());
    result_ += "s";
  }

  /// Print duration types with minute resolution.
  template <class Rep>
  void consume(std::chrono::duration<Rep, std::ratio<60>>& x) {
    result_ += std::to_string(x.count());
    result_ += "min";
  }

  /// Print duration types with hour resolution.
  template <class Rep>
  void consume(std::chrono::duration<Rep, std::ratio<3600>>& x) {
    result_ += std::to_string(x.count());
    result_ += "h";
  }

  /// Fallback printing `<unprintable>`.
  template <class T>
  enable_if_t<
    !is_iterable<T>::value
    && !has_peek_all<T>::value
    && !std::is_pointer<T>::value
    && !is_inspectable<stringification_inspector, T>::value
    && !std::is_arithmetic<T>::value
    && !std::is_convertible<T, string_view>::value
    && !has_to_string<T>::value>
  consume(T&) {
    result_ += "<unprintable>";
  }

  template <class T, class... Ts>
  void traverse(const meta::hex_formatted_t&, const T& x, const Ts&... xs) {
    sep();
    append_hex(result_, reinterpret_cast<uint8_t*>(deconst(x).data()),
               x.size());
    traverse(xs...);
  }

  template <class T, class... Ts>
  void traverse(const meta::omittable_if_none_t&, const T& x, const Ts&... xs) {
    if (x != none) {
      sep();
      consume(x);
    }
    traverse(xs...);
  }

  template <class T, class... Ts>
  void traverse(const meta::omittable_if_empty_t&, const T& x,
                const Ts&... xs) {
    if (!x.empty()) {
      sep();
      consume(x);
    }
    traverse(xs...);
  }

  template <class T, class... Ts>
  void traverse(const meta::omittable_t&, const T&, const Ts&... xs) {
    traverse(xs...);
  }

  template <class... Ts>
  void traverse(const meta::type_name_t& x, const Ts&... xs) {
    sep();
    result_ += x.value;
    result_ += '(';
    traverse(xs...);
    result_ += ')';
  }

  template <class... Ts>
  void traverse(const meta::annotation&, const Ts&... xs) {
    traverse(xs...);
  }

  template <class T, class... Ts>
  enable_if_t<!meta::is_annotation<T>::value && !is_callable<T>::value>
  traverse(const T& x, const Ts&... xs) {
    sep();
    consume(deconst(x));
    traverse(xs...);
  }

  template <class T, class... Ts>
  enable_if_t<!meta::is_annotation<T>::value && is_callable<T>::value>
  traverse(const T&, const Ts&... xs) {
    sep();
    result_ += "<fun>";
    traverse(xs...);
  }

private:
  template <class T>
  T& deconst(const T& x) {
    return const_cast<T&>(x);
  }

  std::string& result_;
};

} // namespace detail
} // namespace caf

