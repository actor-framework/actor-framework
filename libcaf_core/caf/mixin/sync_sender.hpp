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

#ifndef CAF_MIXIN_SYNC_SENDER_HPP
#define CAF_MIXIN_SYNC_SENDER_HPP

#include <tuple>

#include "caf/actor.hpp"
#include "caf/message.hpp"
#include "caf/duration.hpp"
#include "caf/actor_cast.hpp"
#include "caf/response_handle.hpp"
#include "caf/message_priority.hpp"
#include "caf/check_typed_input.hpp"

namespace caf {
namespace mixin {

template <class Base, class Subtype, class HandleTag>
class sync_sender_impl : public Base {
 public:
  using response_handle_type = response_handle<Subtype, message, HandleTag>;

  /****************************************************************************
   *                              sync_send(...)                              *
   ****************************************************************************/

  /**
   * Sends `{vs...}` as a synchronous message to `dest` with priority `prio`.
   * @returns A handle identifying a future-like handle to the response.
   * @warning The returned handle is actor specific and the response to the
   *          sent message cannot be received by another actor.
   * @throws std::invalid_argument if `dest == invalid_actor`
   */
  template <class... Vs>
  response_handle_type sync_send(message_priority prio, const actor& dest,
                                 Vs&&... vs) {
    static_assert(sizeof...(Vs) > 0, "no message to send");
    return {dptr()->sync_send_impl(prio, dest,
                                   make_message(std::forward<Vs>(vs)...)),
            dptr()};
  }

  /**
   * Sends `{vs...}` as a synchronous message to `dest`.
   * @returns A handle identifying a future-like handle to the response.
   * @warning The returned handle is actor specific and the response to the
   *          sent message cannot be received by another actor.
   * @throws std::invalid_argument if `dest == invalid_actor`
   */
  template <class... Vs>
  response_handle_type sync_send(const actor& dest, Vs&&... vs) {
    return sync_send(message_priority::normal, dest, std::forward<Vs>(vs)...);
  }

  /**
   * Sends `{vs...}` as a synchronous message to `dest` with priority `prio`.
   * @returns A handle identifying a future-like handle to the response.
   * @warning The returned handle is actor specific and the response to the
   *          sent message cannot be received by another actor.
   * @throws std::invalid_argument if `dest == invalid_actor`
   */
  template <class... Rs, class... Vs>
  response_handle<Subtype,
                  typename detail::deduce_output_type<
                    detail::type_list<Rs...>,
                    detail::type_list<
                      typename detail::implicit_conversions<
                        typename std::decay<Vs>::type
                      >::type...>
                  >::type,
                  HandleTag>
  sync_send(message_priority prio, const typed_actor<Rs...>& dest, Vs&&... vs) {
    static_assert(sizeof...(Vs) > 0, "no message to send");
    using token =
      detail::type_list<
        typename detail::implicit_conversions<
          typename std::decay<Vs>::type
        >::type...>;
    token tk;
    check_typed_input(dest, tk);
    return {dptr()->sync_send_impl(prio, actor_cast<actor>(dest),
                                   make_message(std::forward<Vs>(vs)...)),
            dptr()};
  }

  /**
   * Sends `{vs...}` as a synchronous message to `dest`.
   * @returns A handle identifying a future-like handle to the response.
   * @warning The returned handle is actor specific and the response to the
   *          sent message cannot be received by another actor.
   * @throws std::invalid_argument if `dest == invalid_actor`
   */
  template <class... Rs, class... Vs>
  response_handle<Subtype,
                  typename detail::deduce_output_type<
                    detail::type_list<Rs...>,
                    detail::type_list<
                      typename detail::implicit_conversions<
                        typename std::decay<Vs>::type
                      >::type...>
                  >::type,
                  HandleTag>
  sync_send(const typed_actor<Rs...>& dest, Vs&&... vs) {
    return sync_send(message_priority::normal, dest, std::forward<Vs>(vs)...);
  }

  /****************************************************************************
   *                           timed_sync_send(...)                           *
   ****************************************************************************/

  /**
   * Sends `{vs...}` as a synchronous message to `dest` with priority `prio`
   * and relative timeout `rtime`.
   * @returns A handle identifying a future-like handle to the response.
   * @warning The returned handle is actor specific and the response to the
   *          sent message cannot be received by another actor.
   * @throws std::invalid_argument if `dest == invalid_actor`
   */
  template <class... Vs>
  response_handle_type timed_sync_send(message_priority prio, const actor& dest,
                                       const duration& rtime, Vs&&... vs) {
    static_assert(sizeof...(Vs) > 0, "no message to send");
    return {dptr()->timed_sync_send_impl(prio, dest, rtime,
                                         make_message(std::forward<Vs>(vs)...)),
            dptr()};
  }

  /**
   * Sends `{vs...}` as a synchronous message to `dest` with
   * relative timeout `rtime`.
   * @returns A handle identifying a future-like handle to the response.
   * @warning The returned handle is actor specific and the response to the
   *          sent message cannot be received by another actor.
   * @throws std::invalid_argument if `dest == invalid_actor`
   */
  template <class... Vs>
  response_handle_type timed_sync_send(const actor& dest, const duration& rtime,
                                       Vs&&... vs) {
    return timed_sync_send(message_priority::normal, dest, rtime,
                           std::forward<Vs>(vs)...);
  }

  /**
   * Sends `{vs...}` as a synchronous message to `dest` with priority `prio`
   * and relative timeout `rtime`.
   * @returns A handle identifying a future-like handle to the response.
   * @warning The returned handle is actor specific and the response to the
   *          sent message cannot be received by another actor.
   * @throws std::invalid_argument if `dest == invalid_actor`
   */
  template <class... Rs, class... Vs>
  response_handle<Subtype,
                  typename detail::deduce_output_type<
                    detail::type_list<Rs...>,
                    typename detail::implicit_conversions<
                      typename std::decay<Vs>::type
                    >::type...
                  >::type,
                  HandleTag>
  timed_sync_send(message_priority prio, const typed_actor<Rs...>& dest,
                  const duration& rtime, Vs&&... vs) {
    static_assert(sizeof...(Vs) > 0, "no message to send");
    using token =
      detail::type_list<
        typename detail::implicit_conversions<
          typename std::decay<Vs>::type
        >::type...>;
    token tk;
    check_typed_input(dest, tk);
    return {dptr()->timed_sync_send_impl(prio, actor_cast<actor>(dest), rtime,
                                         make_message(std::forward<Vs>(vs)...)),
            dptr()};
  }

  /**
   * Sends `{vs...}` as a synchronous message to `dest` with
   * relative timeout `rtime`.
   * @returns A handle identifying a future-like handle to the response.
   * @warning The returned handle is actor specific and the response to the
   *          sent message cannot be received by another actor.
   * @throws std::invalid_argument if `dest == invalid_actor`
   */
  template <class... Rs, class... Vs>
  response_handle<Subtype,
                  typename detail::deduce_output_type<
                    detail::type_list<Rs...>,
                    typename detail::implicit_conversions<
                      typename std::decay<Vs>::type
                    >::type...
                  >::type,
                  HandleTag>
  timed_sync_send(const typed_actor<Rs...>& dest, const duration& rtime,
                  Vs&&... vs) {
    return timed_sync_send(message_priority::normal, dest, rtime,
                           std::forward<Vs>(vs)...);
  }

  // <backward_compatibility version="0.9">
  /****************************************************************************
   *                         outdated member functions                        *
   ****************************************************************************/

  response_handle_type sync_send_tuple(message_priority prio, const actor& dest,
                                       message what) CAF_DEPRECATED;

  response_handle_type sync_send_tuple(const actor& dest,
                                       message what) CAF_DEPRECATED;

  response_handle_type timed_sync_send_tuple(message_priority prio,
                                             const actor& dest,
                                             const duration& rtime,
                                             message what) CAF_DEPRECATED;

  response_handle_type timed_sync_send_tuple(const actor& dest,
                                             const duration& rtime,
                                             message what) CAF_DEPRECATED;
  // </backward_compatibility>

 private:
  Subtype* dptr() {
    return static_cast<Subtype*>(this);
  }
};

template <class ResponseHandleTag>
class sync_sender {
 public:
  template <class Base, class Subtype>
  using impl = sync_sender_impl<Base, Subtype, ResponseHandleTag>;
};

// <backward_compatibility version="0.9">
template <class B, class S, class H>
typename sync_sender_impl<B, S, H>::response_handle_type
sync_sender_impl<B, S, H>::sync_send_tuple(message_priority prio,
                                           const actor& dest, message what) {
  return sync_send(prio, dest, std::move(what));
}

template <class B, class S, class H>
typename sync_sender_impl<B, S, H>::response_handle_type
sync_sender_impl<B, S, H>::sync_send_tuple(const actor& dest, message what) {
  return sync_send(message_priority::normal, dest, std::move(what));
}

template <class B, class S, class H>
typename sync_sender_impl<B, S, H>::response_handle_type
sync_sender_impl<B, S, H>::timed_sync_send_tuple(message_priority prio,
                                                 const actor& dest,
                                                 const duration& rtime,
                                                 message msg) {
  return timed_sync_send(prio, dest, rtime, std::move(msg));
}

template <class B, class S, class H>
typename sync_sender_impl<B, S, H>::response_handle_type
sync_sender_impl<B, S, H>::timed_sync_send_tuple(const actor& dest,
                                                 const duration& rtime,
                                                 message msg) {
  return timed_sync_send(message_priority::normal, dest, rtime, std::move(msg));
}
// </backward_compatibility>

} // namespace mixin
} // namespace caf

#endif // CAF_MIXIN_SYNC_SENDER_HPP
