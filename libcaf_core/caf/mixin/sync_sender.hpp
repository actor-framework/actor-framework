/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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
#include "caf/response_handle.hpp"
#include "caf/message_priority.hpp"
#include "caf/check_typed_input.hpp"

namespace caf {
namespace mixin {

template <class Base, class Subtype, class ResponseHandleTag>
class sync_sender_impl : public Base {

 public:

  using response_handle_type = response_handle<Subtype, message,
                                               ResponseHandleTag>;

  /**************************************************************************
   *           sync_send[_tuple](actor, ...)            *
   **************************************************************************/

  /**
   * Sends `what` as a synchronous message to `whom`.
   * @param dest Receiver of the message.
   * @param what Message content as tuple.
   * @returns A handle identifying a future to the response of `whom`.
   * @warning The returned handle is actor specific and the response to the
   *      sent message cannot be received by another actor.
   * @throws std::invalid_argument if `whom == nullptr
   */
  response_handle_type sync_send_tuple(message_priority prio,
                     const actor& dest, message what) {
    return {dptr()->sync_send_tuple_impl(prio, dest, std::move(what)),
        dptr()};
  }

  response_handle_type sync_send_tuple(const actor& dest, message what) {
    return sync_send_tuple(message_priority::normal, dest, std::move(what));
  }

  /**
   * Sends `{what...}` as a synchronous message to `whom`.
   * @param dest Receiver of the message.
   * @param what Message elements.
   * @returns A handle identifying a future to the response of `whom`.
   * @warning The returned handle is actor specific and the response to the
   *          sent message cannot be received by another actor.
   * @throws std::invalid_argument if `whom == nullptr`
   */
  template <class... Ts>
  response_handle_type sync_send(message_priority prio, const actor& dest,
                   Ts&&... what) {
    static_assert(sizeof...(Ts) > 0, "no message to send");
    return sync_send_tuple(prio, dest,
                 make_message(std::forward<Ts>(what)...));
  }

  template <class... Ts>
  response_handle_type sync_send(const actor& dest, Ts&&... what) {
    static_assert(sizeof...(Ts) > 0, "no message to send");
    return sync_send_tuple(message_priority::normal, dest,
                 make_message(std::forward<Ts>(what)...));
  }

  /**************************************************************************
   *          timed_sync_send[_tuple](actor, ...)           *
   **************************************************************************/

  response_handle_type timed_sync_send_tuple(message_priority prio,
                         const actor& dest,
                         const duration& rtime,
                         message what) {
    return {dptr()->timed_sync_send_tuple_impl(prio, dest, rtime,
                           std::move(what)),
        dptr()};
  }

  response_handle_type timed_sync_send_tuple(const actor& dest,
                         const duration& rtime,
                         message what) {
    return {dptr()->timed_sync_send_tuple_impl(
          message_priority::normal, dest, rtime, std::move(what)),
        dptr()};
  }

  template <class... Ts>
  response_handle_type timed_sync_send(message_priority prio,
                     const actor& dest,
                     const duration& rtime, Ts&&... what) {
    static_assert(sizeof...(Ts) > 0, "no message to send");
    return timed_sync_send_tuple(prio, dest, rtime,
                   make_message(std::forward<Ts>(what)...));
  }

  template <class... Ts>
  response_handle_type timed_sync_send(const actor& dest,
                     const duration& rtime, Ts&&... what) {
    static_assert(sizeof...(Ts) > 0, "no message to send");
    return timed_sync_send_tuple(message_priority::normal, dest, rtime,
                   make_message(std::forward<Ts>(what)...));
  }

  /**************************************************************************
   *        sync_send[_tuple](typed_actor<...>, ...)        *
   **************************************************************************/

  template <class... Rs, class... Ts>
  response_handle<
    Subtype, typename detail::deduce_output_type<
           detail::type_list<Rs...>, detail::type_list<Ts...>>::type,
    ResponseHandleTag>
  sync_send_tuple(message_priority prio, const typed_actor<Rs...>& dest,
          std::tuple<Ts...> what) {
    return sync_send_impl(prio, dest, detail::type_list<Ts...>{},
                          message::move_from_tuple(std::move(what)));
  }

  template <class... Rs, class... Ts>
  response_handle<
    Subtype, typename detail::deduce_output_type<
           detail::type_list<Rs...>, detail::type_list<Ts...>>::type,
    ResponseHandleTag>
  sync_send_tuple(const typed_actor<Rs...>& dest, std::tuple<Ts...> what) {
    return sync_send_impl(message_priority::normal, dest,
                          detail::type_list<Ts...>{},
                          message::move_from_tuple(std::move(what)));
  }

  template <class... Rs, class... Ts>
  response_handle<
    Subtype,
    typename detail::deduce_output_type<
      detail::type_list<Rs...>,
      typename detail::implicit_conversions<
        typename std::decay<Ts>::type>::type...>::type,
    ResponseHandleTag>
  sync_send(message_priority prio, const typed_actor<Rs...>& dest,
        Ts&&... what) {
    return sync_send_impl(
      prio, dest,
      detail::type_list<typename detail::implicit_conversions<
        typename std::decay<Ts>::type>::type...>{},
      make_message(std::forward<Ts>(what)...));
  }

  template <class... Rs, class... Ts>
  response_handle<
    Subtype,
    typename detail::deduce_output_type<
      detail::type_list<Rs...>,
      detail::type_list<typename detail::implicit_conversions<
        typename std::decay<Ts>::type>::type...>>::type,
    ResponseHandleTag>
  sync_send(const typed_actor<Rs...>& dest, Ts&&... what) {
    return sync_send_impl(
      message_priority::normal, dest,
      detail::type_list<typename detail::implicit_conversions<
        typename std::decay<Ts>::type>::type...>{},
      make_message(std::forward<Ts>(what)...));
  }

  /**************************************************************************
   *       timed_sync_send[_tuple](typed_actor<...>, ...)       *
   **************************************************************************/

  /*
  template <class... Rs, class... Ts>
  response_handle<
    Subtype,
    typename detail::deduce_output_type<
      detail::type_list<Rs...>,
      detail::type_list<Ts...>
    >::type,
    ResponseHandleTag
  >
  timed_sync_send_tuple(message_priority prio,
              const typed_actor<Rs...>& dest,
              const duration& rtime,
              cow_tuple<Ts...> what) {
    return {dptr()->timed_sync_send_tuple_impl(prio, dest, rtime,
                           std::move(what)),
        dptr()};
  }

  template <class... Rs, class... Ts>
  response_handle<
    Subtype,
    typename detail::deduce_output_type<
      detail::type_list<Rs...>,
      detail::type_list<Ts...>
    >::type,
    ResponseHandleTag
  >
  timed_sync_send_tuple(const typed_actor<Rs...>& dest,
              const duration& rtime,
              cow_tuple<Ts...> what) {
    return {dptr()->timed_sync_send_tuple_impl(message_priority::normal,
                           dest, rtime,
                           std::move(what)),
        dptr()};
  }
  */

  template <class... Rs, class... Ts>
  response_handle<
    Subtype,
    typename detail::deduce_output_type<
      detail::type_list<Rs...>,
      typename detail::implicit_conversions<
        typename std::decay<Ts>::type>::type...>::type,
    ResponseHandleTag>
  timed_sync_send(message_priority prio, const typed_actor<Rs...>& dest,
          const duration& rtime, Ts&&... what) {
    static_assert(sizeof...(Ts) > 0, "no message to send");
    return timed_sync_send_tuple(prio, dest, rtime,
                   make_message(std::forward<Ts>(what)...));
  }

  template <class... Rs, class... Ts>
  response_handle<
    Subtype,
    typename detail::deduce_output_type<
      detail::type_list<Rs...>,
      typename detail::implicit_conversions<
        typename std::decay<Ts>::type>::type...>::type,
    ResponseHandleTag>
  timed_sync_send(const typed_actor<Rs...>& dest, const duration& rtime,
          Ts&&... what) {
    static_assert(sizeof...(Ts) > 0, "no message to send");
    return timed_sync_send_tuple(message_priority::normal, dest, rtime,
                   make_message(std::forward<Ts>(what)...));
  }

 private:

  template <class... Rs, class... Ts>
  response_handle<
    Subtype, typename detail::deduce_output_type<
           detail::type_list<Rs...>, detail::type_list<Ts...>>::type,
    ResponseHandleTag>
  sync_send_impl(message_priority prio, const typed_actor<Rs...>& dest,
           detail::type_list<Ts...> token, message&& what) {
    check_typed_input(dest, token);
    return {dptr()->sync_send_tuple_impl(prio, dest, std::move(what)),
        dptr()};
  }

  inline Subtype* dptr() { return static_cast<Subtype*>(this); }

};

template <class ResponseHandleTag>
class sync_sender {

 public:

  template <class Base, class Subtype>
  using impl = sync_sender_impl<Base, Subtype, ResponseHandleTag>;

};

} // namespace mixin
} // namespace caf

#endif // CAF_MIXIN_SYNC_SENDER_HPP
