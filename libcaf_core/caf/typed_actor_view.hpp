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

#ifndef CAF_TYPED_ACTOR_VIEW_HPP
#define CAF_TYPED_ACTOR_VIEW_HPP

#include "caf/local_actor.hpp"

namespace caf {

template <class... Sigs>
class typed_actor_view {
public:
  typed_actor_view(local_actor* selfptr) : self_(selfptr) {
    // nop
  }

  /****************************************************************************
   *                           spawn actors                                   *
   ****************************************************************************/

  template <class T, spawn_options Os = no_spawn_options, class... Ts>
  typename infer_handle_from_class<T>::type
  spawn(Ts&&... xs) {
    return self_->spawn<T, Os>(std::forward<Ts>(xs)...);
  }

  template <spawn_options Os = no_spawn_options, class F, class... Ts>
  typename infer_handle_from_fun<F>::type
  spawn(F fun, Ts&&... xs) {
    return self_->spawn<Os>(std::move(fun), std::forward<Ts>(xs)...);
  }

  /****************************************************************************
   *                        send asynchronous messages                        *
   ****************************************************************************/

  template <class... Ts>
  void send(message_priority mp, const actor& dest, Ts&&... xs) {
    self_->send(mp, dest, std::forward<Ts>(xs)...);
  }

  template <class... Ts>
  void send(const actor& dest, Ts&&... xs) {
    self_->send(dest, std::forward<Ts>(xs)...);
  }

  template <class... DestSigs, class... Ts>
  void send(message_priority mp, const typed_actor<DestSigs...>& dest,
            Ts&&... xs) {
    detail::sender_signature_checker<
      detail::type_list<Sigs...>,
      detail::type_list<DestSigs...>,
      detail::type_list<
        typename detail::implicit_conversions<
          typename std::decay<Ts>::type
        >::type...
      >
    >::check();
    self_->send(mp, dest, std::forward<Ts>(xs)...);
  }

  template <class... DestSigs, class... Ts>
  void send(const typed_actor<DestSigs...>& dest, Ts&&... xs) {
    detail::sender_signature_checker<
      detail::type_list<Sigs...>,
      detail::type_list<DestSigs...>,
      detail::type_list<
        typename detail::implicit_conversions<
          typename std::decay<Ts>::type
        >::type...
      >
    >::check();
    self_->send(dest, std::forward<Ts>(xs)...);
  }

  template <class... Ts>
  void delayed_send(message_priority mp, const actor& dest,
                    const duration& rtime, Ts&&... xs) {
    self_->delayed_send(mp, dest, rtime, std::forward<Ts>(xs)...);
  }

  template <class... Ts>
  void delayed_send(const actor& dest, const duration& rtime, Ts&&... xs) {
    self_->delayed_send(dest, rtime, std::forward<Ts>(xs)...);
  }

  template <class... DestSigs, class... Ts>
  void delayed_send(message_priority mp, const typed_actor<DestSigs...>& dest,
                    const duration& rtime, Ts&&... xs) {
    detail::sender_signature_checker<
      detail::type_list<Sigs...>,
      detail::type_list<DestSigs...>,
      detail::type_list<
        typename detail::implicit_conversions<
          typename std::decay<Ts>::type
        >::type...
      >
    >::check();
    self_->delayed_send(mp, dest, rtime, std::forward<Ts>(xs)...);
  }

  template <class... DestSigs, class... Ts>
  void delayed_send(const typed_actor<DestSigs...>& dest,
                    const duration& rtime, Ts&&... xs) {
    detail::sender_signature_checker<
      detail::type_list<Sigs...>,
      detail::type_list<DestSigs...>,
      detail::type_list<
        typename detail::implicit_conversions<
          typename std::decay<Ts>::type
        >::type...
      >
    >::check();
    self_->delayed_send(dest, rtime, std::forward<Ts>(xs)...);
  }

  /****************************************************************************
   *                      miscellaneous actor operations                      *
   ****************************************************************************/

  message& current_message() {
    return self_->current_message();
  }

  actor_system& system() const {
    return self_->system();
  }

  void quit(exit_reason reason = exit_reason::normal) {
    self_->quit(reason);
  }

  template <class... Ts>
  typename detail::make_response_promise_helper<Ts...>::type
  make_response_promise() {
    return self_->make_response_promise<Ts...>();
  }

  template <class T>
  void monitor(const T& x) {
    self_->monitor(x);
  }

  template <class T>
  void demonitor(const T& x) {
    self_->demonitor(x);
  }

  response_promise make_response_promise() {
    return self_->make_response_promise();
  }

  template <class... Ts,
            class R =
              typename detail::make_response_promise_helper<
                typename std::decay<Ts>::type...
              >::type>
  R response(Ts&&... xs) {
    return self_->response(std::forward<Ts>(xs)...);
  }

private:
  local_actor* self_;
};

} // namespace caf

#endif // CAF_TYPED_ACTOR_VIEW_HPP
