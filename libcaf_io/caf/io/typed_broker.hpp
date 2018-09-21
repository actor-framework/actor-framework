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

#include <map>
#include <vector>
#include <type_traits>

#include "caf/none.hpp"
#include "caf/config.hpp"
#include "caf/make_counted.hpp"
#include "caf/extend.hpp"
#include "caf/typed_actor.hpp"
#include "caf/local_actor.hpp"

#include "caf/logger.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/actor_registry.hpp"
#include "caf/detail/sync_request_bouncer.hpp"

#include "caf/mixin/sender.hpp"
#include "caf/mixin/requester.hpp"
#include "caf/mixin/behavior_changer.hpp"

#include "caf/io/fwd.hpp"
#include "caf/io/middleman.hpp"
#include "caf/io/abstract_broker.hpp"

namespace caf {

template <class... Sigs>
class behavior_type_of<io::typed_broker<Sigs...>> {
public:
  using type = typed_behavior<Sigs...>;
};

namespace io {

/// Denotes a minimal "client" broker managing one or more connection
/// handles by reacting to `new_data_msg` and `connection_closed_msg`.
/// @relates typed_broker
using connection_handler = typed_actor<reacts_to<new_data_msg>,
                                       reacts_to<connection_closed_msg>>;

/// Denotes a minimal "server" broker managing one or more accept
/// handles by reacting to `new_connection_msg` and `acceptor_closed_msg`.
/// The accept handler usually calls `self->fork(...)` when receiving
/// a `new_connection_msg`.
/// @relates typed_broker
using accept_handler = typed_actor<reacts_to<new_connection_msg>,
                                   reacts_to<acceptor_closed_msg>>;

/// A typed broker mediates between actor systems and other
/// components in the network.
/// @extends local_actor
template <class... Sigs>
class typed_broker : public extend<abstract_broker,
                                   typed_broker<Sigs...>>::template
                            with<mixin::sender, mixin::requester,
                                 mixin::behavior_changer>,
                     public statically_typed_actor_base {
public:
  using signatures = detail::type_list<Sigs...>;

  using actor_hdl = typed_actor<Sigs...>;

  using behavior_type = typed_behavior<Sigs...>;

  using super = typename extend<abstract_broker, typed_broker<Sigs...>>::
    template with<mixin::sender, mixin::requester, mixin::behavior_changer>;

  /// @cond PRIVATE
  std::set<std::string> message_types() const override {
    detail::type_list<typed_actor<Sigs...>> hdl;
    return this->system().message_types(hdl);
  }

  void initialize() override {
    CAF_LOG_TRACE("");
    this->init_broker();
    auto bhvr = make_behavior();
    CAF_LOG_DEBUG_IF(!bhvr, "make_behavior() did not return a behavior:"
                            << CAF_ARG2("alive", this->alive()));
    if (bhvr) {
      // make_behavior() did return a behavior instead of using become()
      CAF_LOG_DEBUG("make_behavior() did return a valid behavior");
      this->do_become(std::move(bhvr.unbox()), true);
    }
  }

  template <class F, class... Ts>
  typename infer_handle_from_fun<F>::type
  fork(F fun, connection_handle hdl, Ts&&... xs) {
    CAF_ASSERT(this->context() != nullptr);
    auto sptr = this->take(hdl);
    CAF_ASSERT(sptr->hdl() == hdl);
    using impl = typename infer_handle_from_fun<F>::impl;
    static_assert(std::is_convertible<
                    typename impl::actor_hdl,
                    connection_handler
                  >::value,
                  "Cannot fork: new broker misses required handlers");
    actor_config cfg{this->context()};
    detail::init_fun_factory<impl, F> fac;
    cfg.init_fun = fac(std::move(fun), hdl, std::forward<Ts>(xs)...);
    auto res = this->system().spawn_functor(cfg, fun, hdl, std::forward<Ts>(xs)...);
    auto forked = static_cast<impl*>(actor_cast<abstract_actor*>(res));
    forked->move_scribe(std::move(sptr));
    return res;
  }

  expected<connection_handle> add_tcp_scribe(const std::string& host, uint16_t port) {
    static_assert(std::is_convertible<actor_hdl, connection_handler>::value,
                  "Cannot add scribe: broker misses required handlers");
    return super::add_tcp_scribe(host, port);
  }

  connection_handle add_tcp_scribe(network::native_socket fd) {
    static_assert(std::is_convertible<actor_hdl, connection_handler>::value,
                  "Cannot add scribe: broker misses required handlers");
    return super::add_tcp_scribe(fd);
  }

  expected<std::pair<accept_handle, uint16_t>>
  add_tcp_doorman(uint16_t port = 0,
                  const char* in = nullptr,
                  bool reuse_addr = false) {
    static_assert(std::is_convertible<actor_hdl, accept_handler>::value,
                  "Cannot add doorman: broker misses required handlers");
    return super::add_tcp_doorman(port, in, reuse_addr);
  }

  expected<accept_handle> add_tcp_doorman(network::native_socket fd) {
    static_assert(std::is_convertible<actor_hdl, accept_handler>::value,
                  "Cannot add doorman: broker misses required handlers");
    return super::add_tcp_doorman(fd);
  }

  explicit typed_broker(actor_config& cfg) : super(cfg) {
    // nop
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

} // namespace io
} // namespace caf

