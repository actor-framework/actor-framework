// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/behavior.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/timeout_definition.hpp"
#include "caf/timespan.hpp"

namespace caf::detail {

class CAF_CORE_EXPORT blocking_behavior {
public:
  behavior& nested;

  blocking_behavior(behavior& x);
  blocking_behavior(blocking_behavior&&) = default;

  virtual ~blocking_behavior();

  virtual skippable_result fallback(message&);

  virtual timespan timeout();

  virtual void handle_timeout();
};

template <class F>
class blocking_behavior_v3 : public blocking_behavior {
public:
  timeout_definition<F> f;

  blocking_behavior_v3(behavior& x, timeout_definition<F> y)
    : blocking_behavior(x), f(std::move(y)) {
    // nop
  }

  blocking_behavior_v3(blocking_behavior_v3&&) = default;

  timespan timeout() override {
    return f.timeout;
  }

  void handle_timeout() override {
    f.handler();
  }
};

struct make_blocking_behavior_t {
  constexpr make_blocking_behavior_t() {
    // nop
  }

  blocking_behavior operator()(behavior* x) const {
    CAF_ASSERT(x != nullptr);
    return {*x};
  }

  template <class F>
  blocking_behavior_v3<F>
  operator()(behavior* x, timeout_definition<F> y) const {
    CAF_ASSERT(x != nullptr);
    return {*x, std::move(y)};
  }
};

constexpr make_blocking_behavior_t make_blocking_behavior
  = make_blocking_behavior_t{};

} // namespace caf::detail
