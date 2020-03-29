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
#include <functional>
#include <string>
#include <type_traits>
#include <vector>

#include "caf/detail/append_hex.hpp"
#include "caf/detail/apply_args.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/inspect.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/fwd.hpp"
#include "caf/meta/annotation.hpp"
#include "caf/meta/hex_formatted.hpp"
#include "caf/meta/omittable.hpp"
#include "caf/meta/omittable_if_empty.hpp"
#include "caf/meta/omittable_if_none.hpp"
#include "caf/meta/type_name.hpp"
#include "caf/none.hpp"
#include "caf/string_view.hpp"
#include "caf/timespan.hpp"
#include "caf/timestamp.hpp"

namespace caf::detail {

class CAF_CORE_EXPORT stringification_inspector {
public:
  // -- member types required by Inspector concept -----------------------------

  using result_type = void;

  static constexpr bool reads_state = true;
  static constexpr bool writes_state = false;

  // -- constructors, destructors, and assignment operators --------------------

  stringification_inspector(std::string& result) : result_(result) {
    // nop
  }

  // -- serializer interface ---------------------------------------------------

  void begin_object(type_id_t) {
    // nop
  }

  void end_object() {
    // nop
  }

  void begin_sequence(size_t) {
    // nop
  }

  void end_sequence() {
    // nop
  }

  // -- operator() -------------------------------------------------------------

  template <class... Ts>
  void operator()(Ts&&... xs) {
    traverse(xs...);
  }

  /// Prints a separator to the result string.
  void sep();

  void consume(const timespan& x);

  void consume(const timestamp& x);

  void consume(const bool& x);

  void consume(const std::vector<bool>& xs);

  template <class T, size_t N>
  void consume(const T (&xs)[N]) {
    consume_range(xs, xs + N);
  }

  template <class T>
  void consume(const T& x) {
    if constexpr (std::is_pointer<T>::value) {
      consume_ptr(x);
    } else if constexpr (std::is_convertible<T, string_view>::value) {
      consume_str(string_view{x});
    } else if constexpr (std::is_integral<T>::value) {
      if constexpr (std::is_signed<T>::value)
        consume_int(static_cast<int64_t>(x));
      else
        consume_int(static_cast<uint64_t>(x));
    } else if constexpr (std::is_floating_point<T>::value) {
      result_ += std::to_string(x);
    } else if constexpr (has_to_string<T>::value) {
      result_ += to_string(x);
    } else if constexpr (is_inspectable<stringification_inspector, T>::value) {
      inspect(*this, const_cast<T&>(x));
    } else if constexpr (is_map_like<T>::value) {
      result_ += '{';
      for (const auto& kvp : x) {
        sep();
        consume(kvp.first);
        result_ += " = ";
        consume(kvp.second);
      }
      result_ += '}';
    } else if constexpr (is_iterable<T>::value) {
      consume_range(x.begin(), x.end());
    } else if constexpr (has_peek_all<T>::value) {
      result_ += '[';
      x.peek_all(*this);
      result_ += ']';
    } else {
      result_ += "<unprintable>";
    }
  }

  template <class Clock, class Duration>
  void consume(const std::chrono::time_point<Clock, Duration>& x) {
    timestamp tmp{std::chrono::duration_cast<timespan>(x.time_since_epoch())};
    consume(tmp);
  }

  template <class Rep, class Period>
  void consume(const std::chrono::duration<Rep, Period>& x) {
    auto tmp = std::chrono::duration_cast<timespan>(x);
    consume(tmp);
  }

  // Unwrap std::ref.
  template <class T>
  void consume(const std::reference_wrapper<T>& x) {
    return consume(x.get());
  }

  template <class F, class S>
  void consume(const std::pair<F, S>& x) {
    result_ += '(';
    traverse(x.first, x.second);
    result_ += ')';
  }

  template <class... Ts>
  void consume(const std::tuple<Ts...>& x) {
    result_ += '(';
    apply_args(*this, get_indices(x), x);
    result_ += ')';
  }

  void traverse() {
    // end of recursion
  }

  template <class T, class... Ts>
  void traverse(const meta::hex_formatted_t&, const T& x, const Ts&... xs) {
    sep();
    if constexpr (std::is_integral<T>::value) {
      append_hex(result_, x);
    } else {
      static_assert(sizeof(typename T::value_type) == 1);
      append_hex(result_, x.data(), x.size());
    }
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
  void
  traverse(const meta::omittable_if_empty_t&, const T& x, const Ts&... xs) {
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
    consume(x);
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
  template <class Iterator>
  void consume_range(Iterator first, Iterator last) {
    result_ += '[';
    while (first != last) {
      sep();
      consume(*first++);
    }
    result_ += ']';
  }

  template <class T>
  void consume_ptr(const T* ptr) {
    if (ptr) {
      result_ += '*';
      consume(*ptr);
    } else {
      result_ += "nullptr";
    }
  }

  void consume_str(string_view str);

  void consume_ptr(const void* ptr);

  void consume_ptr(const char* cstr);

  void consume_int(int64_t x);

  void consume_int(uint64_t x);

  std::string& result_;
};

} // namespace caf::detail
