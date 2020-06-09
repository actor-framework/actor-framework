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

#include "caf/telemetry/label.hpp"

#include "caf/telemetry/label_view.hpp"

namespace caf::telemetry {

label::label(string_view name, string_view value) : name_length_(name.size()) {
  str_.reserve(name.size() + value.size() + 1);
  str_.insert(str_.end(), name.begin(), name.end());
  str_ += '=';
  str_.insert(str_.end(), value.begin(), value.end());
}

label::label(const label_view& view) : label(view.name(), view.value()) {
  // nop
}

int label::compare(const label& x) const noexcept {
  return str_.compare(x.str());
}

std::string to_string(const label& x) {
  return x.str();
}

} // namespace caf::telemetry
