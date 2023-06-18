// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <string_view>

#include "caf/detail/comparable.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/hash/fnv.hpp"

namespace caf::telemetry {

class CAF_CORE_EXPORT label_view : detail::comparable<label_view>,
                                   detail::comparable<label_view, label> {
public:
  // -- constructors, destructors, and assignment operators --------------------

  label_view() = delete;

  label_view(const label_view&) = default;

  label_view& operator=(const label_view&) = default;

  /// @pre `key` matches the regex `[a-zA-Z_:][a-zA-Z0-9_:]*`
  label_view(std::string_view name, std::string_view value)
    : name_(name), value_(value) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  std::string_view name() const noexcept {
    return name_;
  }

  std::string_view value() const noexcept {
    return value_;
  }

  // -- comparison -------------------------------------------------------------

  int compare(const label& other) const noexcept;

  int compare(const label_view& other) const noexcept;

private:
  std::string_view name_;
  std::string_view value_;
};

/// Returns the @ref label_view in `name=value` notation.
/// @relates label_view
CAF_CORE_EXPORT std::string to_string(const label_view& x);

} // namespace caf::telemetry

namespace std {

template <>
struct hash<caf::telemetry::label_view> {
  size_t operator()(const caf::telemetry::label_view& x) const noexcept {
    return caf::hash::fnv<size_t>::compute(x.name(), '=', x.value());
  }
};

} // namespace std
