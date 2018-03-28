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

#include "caf/local_actor.hpp"

#include "caf/detail/invoke_result_visitor.hpp"

namespace caf {
namespace detail {

template <class Self>
class default_invoke_result_visitor : public invoke_result_visitor {
public:
  inline default_invoke_result_visitor(Self* ptr) : self_(ptr) {
    // nop
  }

  ~default_invoke_result_visitor() override {
    // nop
  }

  void operator()() override {
    // nop
  }

  void operator()(error& x) override {
    CAF_LOG_TRACE(CAF_ARG(x));
    delegate(x);
  }

  void operator()(message& x) override {
    CAF_LOG_TRACE(CAF_ARG(x));
    delegate(x);
  }

  void operator()(const none_t& x) override {
    CAF_LOG_TRACE(CAF_ARG(x));
    delegate(x);
  }

private:
  void deliver(response_promise& rp, error& x) {
    CAF_LOG_DEBUG("report error back to requesting actor");
    rp.deliver(std::move(x));
  }

  void deliver(response_promise& rp, message& x) {
    CAF_LOG_DEBUG("respond via response_promise");
    // suppress empty messages for asynchronous messages
    if (x.empty() && rp.async())
      return;
    rp.deliver(std::move(x));
  }

  void deliver(response_promise& rp, const none_t&) {
    error err = sec::unexpected_response;
    deliver(rp, err);
  }

  template <class T>
  void delegate(T& x) {
    auto rp = self_->make_response_promise();
    if (!rp.pending()) {
      CAF_LOG_DEBUG("suppress response message: invalid response promise");
      return;
    }
    deliver(rp, x);
  }

  Self* self_;
};

} // namespace detail
} // namespace caf

