// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/intrusive_ptr.hpp"
#include "caf/ref_counted.hpp"

namespace caf::detail {

template <class Base>
class embedded final : public Base {
public:
  template <class... Ts>
  embedded(intrusive_ptr<ref_counted> storage, Ts&&... xs)
    : Base(std::forward<Ts>(xs)...), storage_(std::move(storage)) {
    // nop
  }

  ~embedded() {
    // nop
  }

  void request_deletion(bool) noexcept override {
    intrusive_ptr<ref_counted> guard;
    guard.swap(storage_);
    // this code assumes that embedded is part of pair_storage<>,
    // i.e., this object lives inside a union!
    this->~embedded();
  }

protected:
  intrusive_ptr<ref_counted> storage_;
};

} // namespace caf::detail
