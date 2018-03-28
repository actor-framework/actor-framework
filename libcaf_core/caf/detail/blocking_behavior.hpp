/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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

#include "caf/behavior.hpp"
#include "caf/catch_all.hpp"
#include "caf/timeout_definition.hpp"

namespace caf {
namespace detail {

class blocking_behavior {
public:
  behavior& nested;

  blocking_behavior(behavior& x);
  blocking_behavior(blocking_behavior&&) = default;

  virtual ~blocking_behavior();

  virtual result<message> fallback(message_view&);

  virtual duration timeout();

  virtual void handle_timeout();
};

template <class F>
class blocking_behavior_v2 : public blocking_behavior {
public:
  catch_all<F> f;

  blocking_behavior_v2(behavior& x, catch_all<F> y)
      : blocking_behavior(x),
        f(std::move(y)) {
    // nop
  }

  blocking_behavior_v2(blocking_behavior_v2&&) = default;

  result<message> fallback(message_view& x) override {
    return f.handler(x);
  }
};

template <class F>
class blocking_behavior_v3 : public blocking_behavior {
public:
  timeout_definition<F> f;

  blocking_behavior_v3(behavior& x, timeout_definition<F> y)
      : blocking_behavior(x),
        f(std::move(y)) {
    // nop
  }

  blocking_behavior_v3(blocking_behavior_v3&&) = default;

  duration timeout() override {
    return f.timeout;
  }

  void handle_timeout() override {
    f.handler();
  }
};

template <class F1, class F2>
class blocking_behavior_v4 : public blocking_behavior {
public:
  catch_all<F1> f1;
  timeout_definition<F2> f2;

  blocking_behavior_v4(behavior& x, catch_all<F1> y, timeout_definition<F2> z)
      : blocking_behavior(x),
        f1(std::move(y)),
        f2(std::move(z)) {
    // nop
  }

  blocking_behavior_v4(blocking_behavior_v4&&) = default;

  result<message> fallback(message_view& x) override {
    return f1.handler(x);
  }

  duration timeout() override {
    return f2.timeout;
  }

  void handle_timeout() override {
    f2.handler();
  }
};

struct make_blocking_behavior_t {
  constexpr make_blocking_behavior_t() {
    // nop
  }

  inline blocking_behavior operator()(behavior* x) const {
    CAF_ASSERT(x != nullptr);
    return {*x};
  }

  template <class F>
  blocking_behavior_v2<F> operator()(behavior* x, catch_all<F> y) const {
    CAF_ASSERT(x != nullptr);
    return {*x, std::move(y)};
  }

  template <class F>
  blocking_behavior_v3<F> operator()(behavior* x,
                                     timeout_definition<F> y) const {
    CAF_ASSERT(x != nullptr);
    return {*x, std::move(y)};
  }

  template <class F1, class F2>
  blocking_behavior_v4<F1, F2> operator()(behavior* x, catch_all<F1> y,
                                          timeout_definition<F2> z) const {
    CAF_ASSERT(x != nullptr);
    return {*x, std::move(y), std::move(z)};
  }
};

constexpr make_blocking_behavior_t make_blocking_behavior = make_blocking_behavior_t{};

} // namespace detail
} // namespace caf

