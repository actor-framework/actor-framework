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

#include "caf/replies_to.hpp"
#include "caf/local_actor.hpp"
#include "caf/typed_actor.hpp"
#include "caf/actor_system.hpp"
#include "caf/typed_behavior.hpp"
#include "caf/scheduled_actor.hpp"

#include "caf/mixin/requester.hpp"
#include "caf/mixin/behavior_changer.hpp"

namespace caf {

template <class... Sigs>
class behavior_type_of<typed_event_based_actor<Sigs...>> {
public:
  using type = typed_behavior<Sigs...>;
};

/// A cooperatively scheduled, event-based actor
/// implementation with static type-checking.
/// @extends local_actor
template <class... Sigs>
class typed_event_based_actor : public extend<scheduled_actor,
                                              typed_event_based_actor<Sigs...>
                                       >::template
                                       with<mixin::sender, mixin::requester,
                                            mixin::behavior_changer>,
                                public statically_typed_actor_base {
public:
  using super = typename
                extend<scheduled_actor,
                       typed_event_based_actor<Sigs...>>::template
                with<mixin::sender, mixin::requester, mixin::behavior_changer>;

  explicit typed_event_based_actor(actor_config& cfg) : super(cfg) {
    // nop
  }

  using signatures = detail::type_list<Sigs...>;

  using behavior_type = typed_behavior<Sigs...>;

  using actor_hdl = typed_actor<Sigs...>;

  std::set<std::string> message_types() const override {
    detail::type_list<typed_actor<Sigs...>> token;
    return this->system().message_types(token);
  }

  void initialize() override {
    CAF_LOG_TRACE("");
    super::initialize();
    this->setf(abstract_actor::is_initialized_flag);
    auto bhvr = make_behavior();
    CAF_LOG_DEBUG_IF(!bhvr, "make_behavior() did not return a behavior:"
                            << CAF_ARG2("alive", this->alive()));
    if (bhvr) {
      // make_behavior() did return a behavior instead of using become()
      CAF_LOG_DEBUG("make_behavior() did return a valid behavior");
      this->do_become(std::move(bhvr.unbox()), true);
    }
  }

protected:
  virtual behavior_type make_behavior() {
    if (this->initial_behavior_fac_) {
      auto bhvr = this->initial_behavior_fac_(this);
      this->initial_behavior_fac_ = nullptr;
      if (bhvr)
        this->do_become(std::move(bhvr), true);
    }
    return behavior_type::make_empty_behavior();
  }
};

} // namespace caf

