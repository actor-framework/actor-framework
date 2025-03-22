// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/save_inspector_base.hpp"

#include <cstddef>

namespace caf::detail {

class CAF_CORE_EXPORT stringification_inspector
  : public save_inspector_base<stringification_inspector> {
public:
  // -- member types -----------------------------------------------------------

  using super = save_inspector_base<stringification_inspector>;

  // -- constructors, destructors, and assignment operators --------------------

  stringification_inspector(std::string& result);

  ~stringification_inspector() override;

  // -- properties -------------------------------------------------------------

  bool has_human_readable_format() const noexcept;

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

  bool begin_tuple(size_t size);

  bool end_tuple();

  bool begin_key_value_pair();

  bool end_key_value_pair();

  bool begin_sequence(size_t size);

  bool end_sequence();

  bool begin_associative_array(size_t size);

  bool end_associative_array();

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

  bool value(void* x);

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
  std::enable_if_t<
    has_to_string_v<T> && !std::is_convertible_v<T, std::string_view>, bool>
  builtin_inspect(const T& x) {
    auto str = to_string(x);
    if constexpr (std::is_convertible<decltype(str), const char*>::value) {
      const char* cstr = str;
      append(cstr);
    } else {
      append(str);
    }
    return true;
  }

  template <class T>
  bool builtin_inspect(const T* x) {
    if (x == nullptr) {
      append("null");
      return true;
    }
    if constexpr (std::is_same_v<T, char>) {
      return value(std::string_view{x, strlen(x)});
    } else if constexpr (std::is_same_v<T, void>) {
      return value(x);
    } else {
      append("*");
      save(*this, detail::as_mutable_ref(*x));
      return true;
    }
  }

  template <class T>
  bool builtin_inspect(const std::optional<T>& x) {
    if (!x) {
      append("null");
    } else {
      append("*");
      save(*this, detail::as_mutable_ref(*x));
    }
    return true;
  }

  // -- fallbacks --------------------------------------------------------------

  template <class T>
  bool opaque_value(T& val) {
    if constexpr (detail::is_iterable<T>::value) {
      begin_sequence(val.size());
      for (const auto& elem : val) {
        save(*this, elem);
      }
      end_sequence();
    } else {
      append("<unprintable>");
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
  /// Storage for the implementation object.
  alignas(std::max_align_t) std::byte impl_[64];

  void append(std::string_view str);

  bool int_value(int64_t x);

  bool int_value(uint64_t x);
};

} // namespace caf::detail
