/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/detail/behavior_impl.hpp"

#include "caf/message_handler.hpp"

namespace caf {
namespace detail {

namespace {

class combinator final : public behavior_impl {
public:
  match_case::result invoke(detail::invoke_result_visitor& f, message& arg) override {
    auto x = first->invoke(f, arg);
    return x == match_case::no_match ? second->invoke(f, arg) : x;
  }

  void handle_timeout() override {
    // the second behavior overrides the timeout handling of
    // first behavior
    return second->handle_timeout();
  }

  pointer copy(const generic_timeout_definition& tdef) const override {
    return new combinator(first, second->copy(tdef));
  }

  combinator(const pointer& p0, const pointer& p1)
      : behavior_impl(p1->timeout()),
        first(p0),
        second(p1) {
    // nop
  }

private:
  pointer first;
  pointer second;
};

class maybe_message_visitor : public detail::invoke_result_visitor {
public:
  optional<message> value;

  void operator()() override {
    value = message{};
  }

  void operator()(error& x) override {
    value = make_message(std::move(x));
  }

  void operator()(message& x) override {
    value = std::move(x);
  }

  void operator()(const none_t&) override {
    (*this)();
  }
};

} // namespace <anonymous>

behavior_impl::~behavior_impl() {
  // nop
}

behavior_impl::behavior_impl(duration tout)
    : timeout_(tout),
      begin_(nullptr),
      end_(nullptr) {
  // nop
}

match_case::result behavior_impl::invoke(detail::invoke_result_visitor& f,
                                         message& msg) {
  auto msg_token = msg.type_token();
  for (auto i = begin_; i != end_; ++i)
    if (i->type_token == msg_token)
      switch (i->ptr->invoke(f, msg.cvals().get())) {
        case match_case::no_match:
          break;
        case match_case::match:
          return match_case::match;
        case match_case::skip:
          return match_case::skip;
      };
  return match_case::no_match;
}

optional<message> behavior_impl::invoke(message& x) {
  maybe_message_visitor f;
  invoke(f, x);
  return std::move(f.value);
}

void behavior_impl::handle_timeout() {
  // nop
}

behavior_impl::pointer behavior_impl::or_else(const pointer& other) {
  CAF_ASSERT(other != nullptr);
  return make_counted<combinator>(this, other);
}

} // namespace detail
} // namespace caf
