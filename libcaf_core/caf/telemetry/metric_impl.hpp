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
#include <utility>
#include <vector>

#include "caf/fwd.hpp"
#include "caf/telemetry/label.hpp"
#include "caf/telemetry/metric.hpp"

namespace caf::telemetry {

template <class Type>
class metric_impl : public metric {
public:
  using family_setting = typename Type::family_setting;

  template <class... Ts>
  explicit metric_impl(std::vector<label> labels, Ts&&... xs)
    : metric(std::move(labels)), impl_(this->labels_, std::forward<Ts>(xs)...) {
    // nop
  }

  Type& impl() noexcept {
    return impl_;
  }

  const Type& impl() const noexcept {
    return impl_;
  }

  Type* impl_ptr() noexcept {
    return &impl_;
  }

  const Type* impl_ptr() const noexcept {
    return &impl_;
  }

private:
  Type impl_;
};

} // namespace caf::telemetry
