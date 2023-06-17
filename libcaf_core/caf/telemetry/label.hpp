// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <string>
#include <string_view>

#include "caf/detail/comparable.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/hash/fnv.hpp"

namespace caf::telemetry {

/// An (immutable) key-value pair for adding extra dimensions to metrics.
class CAF_CORE_EXPORT label : detail::comparable<label> {
public:
  // -- constructors, destructors, and assignment operators --------------------

  label() = delete;

  label(label&&) = default;

  label(const label&) = default;

  label& operator=(label&&) = default;

  label& operator=(const label&) = default;

  /// @pre `name` matches the regex `[a-zA-Z_:][a-zA-Z0-9_:]*`
  label(std::string_view name, std::string_view value);

  explicit label(const label_view& view);

  // -- properties -------------------------------------------------------------

  std::string_view name() const noexcept {
    return {str_.data(), name_length_};
  }

  std::string_view value() const noexcept {
    return {str_.data() + name_length_ + 1, str_.size() - name_length_ - 1};
  }

  void value(std::string_view new_value);

  /// Returns the label in `name=value` notation.
  const std::string& str() const noexcept {
    return str_;
  }

  // -- comparison -------------------------------------------------------------

  int compare(const label_view& x) const noexcept;

  int compare(const label& x) const noexcept;

private:
  size_t name_length_;
  std::string str_;
};

/// Returns the @ref label in `name=value` notation.
/// @relates label
CAF_CORE_EXPORT std::string to_string(const label& x);

} // namespace caf::telemetry

namespace std {

template <>
struct hash<caf::telemetry::label> {
  size_t operator()(const caf::telemetry::label& x) const noexcept {
    return caf::hash::fnv<size_t>::compute(x.str());
  }
};

} // namespace std
