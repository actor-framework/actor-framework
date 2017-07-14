/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#ifndef CAF_COMPOSED_STATE_HPP
#define CAF_COMPOSED_STATE_HPP

#include "caf/param.hpp"
#include "caf/composable_behavior.hpp"
#include "caf/typed_actor_pointer.hpp"

namespace caf {

template <class... Ts>
class composed_behavior : public Ts... {
public:
  using signatures =
    typename detail::tl_union<typename Ts::signatures...>::type;

  using handle_type =
    typename detail::tl_apply<
      signatures,
      typed_actor
    >::type;

  using behavior_type = typename handle_type::behavior_type;

  using actor_base = typename handle_type::base;

  using broker_base = typename handle_type::broker_base;

  using self_pointer =
    typename detail::tl_apply<
      signatures,
      typed_actor_pointer
    >::type;

  composed_behavior() : self(nullptr) {
    // nop
  }

  template <class SelfPointer>
  unit_t init_selfptr(SelfPointer x) {
    CAF_ASSERT(x != nullptr);
    self = x;
    return unit(static_cast<Ts*>(this)->init_selfptr(x)...);
  }

  void init_behavior(message_handler& x) override {
    init_behavior_impl(x);
  }

  unit_t init_behavior_impl(message_handler& x) {
    return unit(static_cast<Ts*>(this)->init_behavior_impl(x)...);
  }

protected:
  self_pointer self;
};

} // namespace caf

#endif // CAF_COMPOSED_STATE_HPP
