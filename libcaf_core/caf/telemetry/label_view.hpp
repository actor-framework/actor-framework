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

#include "caf/detail/comparable.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/hash/fnv.hpp"
#include "caf/string_view.hpp"

namespace caf::telemetry {

class CAF_CORE_EXPORT label_view : detail::comparable<label_view>,
                                   detail::comparable<label_view, label> {
public:
  // -- constructors, destructors, and assignment operators --------------------

  label_view() = delete;

  label_view(const label_view&) = default;

  label_view& operator=(const label_view&) = default;

  /// @pre `key` matches the regex `[a-zA-Z_:][a-zA-Z0-9_:]*`
  label_view(string_view name, string_view value) : name_(name), value_(value) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  string_view name() const noexcept {
    return name_;
  }

  string_view value() const noexcept {
    return value_;
  }

  // -- comparison -------------------------------------------------------------

  int compare(const label& x) const noexcept;

  int compare(const label_view& x) const noexcept;

private:
  string_view name_;
  string_view value_;
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
