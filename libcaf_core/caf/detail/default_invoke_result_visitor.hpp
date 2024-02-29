// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/invoke_result_visitor.hpp"
#include "caf/local_actor.hpp"
#include "caf/log/core.hpp"

namespace caf::detail {

template <class Self>
class default_invoke_result_visitor : public invoke_result_visitor {
public:
  using super = invoke_result_visitor;

  default_invoke_result_visitor(Self* ptr) : self_(ptr) {
    // nop
  }

  ~default_invoke_result_visitor() override {
    // nop
  }

  using super::operator();

  void operator()(error& x) override {
    auto lg = log::core::trace("x = {}", x);
    self_->respond(x);
  }

  void operator()(message& x) override {
    auto lg = log::core::trace("x = {}", x);
    self_->respond(x);
  }

private:
  Self* self_;
};

} // namespace caf::detail
