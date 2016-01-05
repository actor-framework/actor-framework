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
  void handle_timeout() override {
    // the second behavior overrides the timeout handling of first behavior
    return second_->handle_timeout();
  }

  pointer copy(const generic_timeout_definition& tdef) const override {
    return new combinator(first_, second_->copy(tdef));
  }

  combinator(const pointer& x, const pointer& y)
      : behavior_impl(y->timeout()),
        first_(x),
        second_(y) {
    cases_.reserve(x->size() + y->size());
    for (auto mci : *x)
      cases_.push_back(mci);
    for (auto mci : *y)
      cases_.push_back(mci);
    begin_ = &cases_[0];
    end_ = begin_ + cases_.size();
  }

private:
  std::vector<match_case_info> cases_;
  pointer first_;
  pointer second_;
};

} // namespace <anonymous>

behavior_impl::~behavior_impl() {
  // nop
}

behavior_impl::behavior_impl(duration tout) : timeout_(tout) {
  // nop
}

bhvr_invoke_result behavior_impl::invoke(message& msg) {
  auto msg_token = msg.type_token();
  bhvr_invoke_result res;
  for (auto i = begin_; i != end_; ++i)
    if ((i->has_wildcard || i->type_token == msg_token)
        && i->ptr->invoke(res, msg) != match_case::no_match)
      return res;
  return none;
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
