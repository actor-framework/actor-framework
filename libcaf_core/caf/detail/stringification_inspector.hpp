// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/inspector_access.hpp"
#include "caf/save_inspector_base.hpp"
#include "caf/timespan.hpp"
#include "caf/timestamp.hpp"

#include <chrono>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

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

  void set_error(error stop_reason) override;

  error& get_error() noexcept override;

  bool begin_object(type_id_t, std::string_view name);

  bool end_object();

  bool begin_field(std::string_view);

  bool begin_field(std::string_view name, bool is_present);

  bool begin_field(std::string_view name, span<const type_id_t>, size_t);

  bool begin_field(std::string_view name, bool, span<const type_id_t>, size_t);

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

  bool value(std::byte x);

  bool value(bool x);

  template <class Integral>
  std::enable_if_t<std::is_integral_v<Integral>, bool> value(Integral x) {
    if constexpr (std::is_signed_v<Integral>)
      return int_value(static_cast<int64_t>(x));
    else
      return int_value(static_cast<uint64_t>(x));
  }

  bool value(float x);

  bool value(double x);

  bool value(long double x);

  bool value(timespan x);

  bool value(timestamp x);

  bool value(std::string_view x);

  bool value(const std::u16string& x);

  bool value(const std::u32string& x);

  bool value(const_byte_span x);

  bool value(const strong_actor_ptr& ptr);

  bool value(const weak_actor_ptr& ptr);

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
  std::enable_if_t<
    has_to_string_v<T> && !std::is_convertible_v<T, std::string_view>, bool>
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
    }
    if constexpr (std::is_same_v<T, char>) {
      return value(std::string_view{x, strlen(x)});
    } else if constexpr (std::is_same_v<T, void>) {
      sep();
      result_ += "*<";
      auto addr = reinterpret_cast<intptr_t>(x);
      result_ += std::to_string(addr);
      result_ += '>';
      return true;
    } else {
      sep();
      result_ += '*';
      save(*this, detail::as_mutable_ref(*x));
      return true;
    }
  }

  template <class T>
  bool builtin_inspect(const std::optional<T>& x) {
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

  // -- convenience API --------------------------------------------------------

  template <class T>
  static std::string render(const T& x) {
    if constexpr (std::is_same_v<std::nullptr_t, T>) {
      return "null";
    } else if constexpr (std::is_constructible_v<std::string_view, T>) {
      if constexpr (std::is_pointer_v<T>) {
        if (x == nullptr)
          return "null";
      }
      auto str = std::string_view{x};
      return std::string{str.begin(), str.end()};
    } else {
      std::string result;
      stringification_inspector f{result};
      save(f, detail::as_mutable_ref(x));
      return result;
    }
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

  error err_;
};

} // namespace caf::detail
