// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/concepts.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/expected.hpp"
#include "caf/fwd.hpp"
#include "caf/save_inspector_base.hpp"
#include "caf/serializer.hpp"

#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <optional>
#include <string_view>

namespace caf::detail {

class CAF_CORE_EXPORT stringification_inspector final
  : public save_inspector_base<stringification_inspector, serializer> {
public:
  using super = save_inspector_base<stringification_inspector, serializer>;

  explicit stringification_inspector(std::string& result);

  stringification_inspector(const stringification_inspector&) = delete;

  stringification_inspector& operator=(const stringification_inspector&)
    = delete;

  ~stringification_inspector() noexcept override;

  // -- properties -------------------------------------------------------------

  [[nodiscard]] bool has_human_readable_format() const noexcept {
    return true;
  }

  using super::value;

  bool value(const void* ptr);

  // -- builtin inspection to pick up to_string or provide nicer formatting ---

  template <class Rep, class Period>
  bool builtin_inspect(const std::chrono::duration<Rep, Period> x) {
    std::string str;
    detail::print(str, x);
    append(str);
    return true;
  }

  template <class Duration>
  bool builtin_inspect(
    const std::chrono::time_point<std::chrono::system_clock, Duration> x) {
    std::string str;
    detail::print(str, x);
    append(str);
    return true;
  }

  template <class T>
    requires(has_to_string<T> && !std::convertible_to<T, std::string_view>)
  bool builtin_inspect(const T& x) {
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

  template <class T>
  bool builtin_inspect(const caf::expected<T>& x) {
    if (x.has_value()) {
      if constexpr (std::is_same_v<T, void>) {
        append("unit");
      } else {
        save(*this, detail::as_mutable_ref(*x));
      }
    } else {
      append("!" + to_string(x.error()));
    }
    return true;
  }

  // -- fallbacks --------------------------------------------------------------

  template <class T>
  bool opaque_value(T& val) {
    if constexpr (detail::iterable<std::remove_const_t<T>>) {
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

  void append(std::string_view str);

private:
  static constexpr size_t impl_storage_size = 96;

  alignas(std::max_align_t) std::byte impl_storage_[impl_storage_size];
};

} // namespace caf::detail
