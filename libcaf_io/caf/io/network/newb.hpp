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

#include "caf/scheduled_actor.hpp"
#include "caf/detail/type_list.hpp"

#include "caf/mixin/sender.hpp"
#include "caf/mixin/requester.hpp"
#include "caf/mixin/behavior_changer.hpp"

#include "caf/io/network/event_handler.hpp"
#include "caf/io/network/default_multiplexer.hpp"

namespace caf {

namespace io {
namespace network {

template <class T, class... Xs>
class newb;

} // namespace network
} // namespace io

template <class T, class... Xs>
class behavior_type_of<io::network::newb<T, Xs...>> {
public:
  using type = behavior;
};


namespace detail {

// Policies provide a newb with functionality. A protocol policy implements a
// network protocol and provides implementations to read and write data as well
// as for forking to new event handlers.
//
// After receiving data the policy stack of a newb is called in order to
struct protocol_policy {
  protocol_policy(int i)
      : idx{i} {
    // nop
  }

  void read() {

  }

  void write() {

  }

  void fork() {

  }
  int idx;
};

struct mutating_policy {
  mutating_policy(int i)
      : idx{i} {
    // nop
  }

  // Call on incoming data. Should return either a bool or a enum to
  // know if furhter policies are called and how to continue processing.
  void handle() {

  }

  int idx;
};

template <class T>
struct is_protocol_policy_type : std::is_base_of<protocol_policy, T> {};

template <class T>
struct is_mutating_policy_type : std::is_base_of<mutating_policy, T> {};

} // namespace detail

namespace io {
namespace network {

/**
 * TODO:
 *  - [x] create a class `newb` that is an event handler and actor
 *  - [x] get it running in the multiplexer
 *  - [ ] create a base policy class
 *  - [x] build `make_policy` which creates a `newb` with multiple policies
 *  - [ ] is there a difference between a protocol policy and a guarantee?
 *  - [ ] get a call hierarchy in both directions
 *  - [ ] what should policy their constrcutors do?
 *  - [ ] ...
 */

template <class Protocol, class... Policies>
class newb : public extend<scheduled_actor,
                           newb<Protocol, Policies...>>::template
                    with<mixin::sender, mixin::requester,
                         mixin::behavior_changer>,
             public dynamically_typed_actor_base,
             public event_handler {
public:
  using super = typename extend<scheduled_actor, newb<Protocol, Policies...>>::
    template with<mixin::sender, mixin::requester, mixin::behavior_changer>;

  using signatures = none_t;

  // -- constructors and destructors -------------------------------------------

  newb(actor_config& cfg, default_multiplexer& dm, native_socket sockfd)
      : super(cfg),
        event_handler(dm, sockfd),
        protocol_{0},
        policies_{Policies{1}...} {
    // nop
  }

  // -- overridden modifiers of abstract_actor ---------------------------------

  void enqueue(mailbox_element_ptr ptr, execution_unit*) override {
    CAF_PUSH_AID(id());
    scheduled_actor::enqueue(std::move(ptr), &backend());
  }

  void enqueue(strong_actor_ptr src, message_id mid, message msg,
               execution_unit*) override {
    enqueue(make_mailbox_element(std::move(src), mid, {}, std::move(msg)),
            &backend());
  }

  // -- overridden modifiers of local_actor ------------------------------------

  void launch(execution_unit* eu, bool lazy, bool hide) override {
    CAF_PUSH_AID_FROM_PTR(this);
    CAF_ASSERT(eu != nullptr);
    CAF_ASSERT(eu == &backend());
    CAF_LOG_TRACE(CAF_ARG(lazy) << CAF_ARG(hide));
    // add implicit reference count held by middleman/multiplexer
    if (!hide)
      super::register_at_system();
    if (lazy && super::mailbox().try_block())
      return;
    intrusive_ptr_add_ref(super::ctrl());
    eu->exec_later(this);
  }

  void initialize() override {
    CAF_LOG_TRACE("");
    init_newb();
    auto bhvr = make_behavior();
    CAF_LOG_DEBUG_IF(!bhvr, "make_behavior() did not return a behavior:"
                             << CAF_ARG(has_behavior()));
    if (bhvr) {
      // make_behavior() did return a behavior instead of using become()
      CAF_LOG_DEBUG("make_behavior() did return a valid behavior");
      this->become(std::move(bhvr));
    }
  }

  // -- overridden modifiers of abstract_broker --------------------------------

  bool cleanup(error&& reason, execution_unit* host) override {
    CAF_LOG_TRACE(CAF_ARG(reason));
    // TODO: Ask policies, close socket.
    return local_actor::cleanup(std::move(reason), host);
  }

  // -- overridden modifiers of resumable --------------------------------------

  multiplexer::runnable::resume_result resume(execution_unit* ctx,
                                              size_t mt) override {
    CAF_ASSERT(ctx != nullptr);
    CAF_ASSERT(ctx == &backend());
    return scheduled_actor::resume(ctx, mt);
  }

  // -- overridden modifiers of event handler ----------------------------------

  void handle_event(operation op) override {
    std::cout << "handling event " << to_string(op) << std::endl;
  }

  void removed_from_loop(operation op) override {
    std::cout << "removing myself from the loop for "
              << to_string(op) << std::endl;
  }

  // -- members ----------------------------------------------------------------

  /// Returns the `multiplexer` running this broker.
  network::multiplexer& backend() {
    return super::system().middleman().backend();
  }

  behavior make_behavior() {
    std::cout << "creating newb behavior" << std::endl;
    return {
      [](int i) {
        std::cout << "got message " << i << std::endl;
      }
    };
  }

  void init_newb() {
    CAF_LOG_TRACE("");
    super::setf(super::is_initialized_flag);
  }

  /// @cond PRIVATE

  template <class... Ts>
  void eq_impl(message_id mid, strong_actor_ptr sender,
               execution_unit* ctx, Ts&&... xs) {
    enqueue(make_mailbox_element(std::move(sender), mid,
                                 {}, std::forward<Ts>(xs)...),
            ctx);
  }

  /// @endcond

private:
  Protocol protocol_;
  std::tuple<Policies...> policies_;
};

template <class Protocol, class... Policies>
actor make_newb(actor_system& sys, actor_config& cfg, default_multiplexer& mpx,
                native_socket sockfd) {
  using policy_types = detail::type_list<Policies...>;
  static_assert(detail::tl_forall<policy_types, detail::is_mutating_policy_type>::value,
                "Only network policies allowed as template parameters");
  static_assert(std::is_base_of<detail::protocol_policy, Protocol>::value,
                "First template argument must be a protocol policy");
  using newb_t = newb<Protocol, Policies...>;
  auto res = sys.spawn_impl<newb_t, hidden>(cfg, mpx, sockfd);
  return actor_cast<actor>(res);
}

} // namespace network
} // namespace io
} // namespace caf
