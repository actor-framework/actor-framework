/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
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

#include <string>

#include "caf/detail/comparable.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/hash/fnv.hpp"
#include "caf/string_view.hpp"

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
  label(string_view name, string_view value);

  explicit label(const label_view& view);

  // -- properties -------------------------------------------------------------

  string_view name() const noexcept {
    return string_view{str_.data(), name_length_};
  }

  string_view value() const noexcept {
    return string_view{str_.data() + name_length_ + 1,
                       str_.size() - name_length_ - 1};
  }

  /// Returns the label in `name=value` notation.
  const std::string& str() const noexcept {
    return str_;
  }

  // -- comparison -------------------------------------------------------------

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
