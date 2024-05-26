// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/fwd.hpp"
#include "caf/telemetry/label.hpp"
#include "caf/telemetry/metric.hpp"

#include <string>
#include <utility>
#include <vector>

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
