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

#ifndef CAF_RESPONSE_HANDLE_HPP
#define CAF_RESPONSE_HANDLE_HPP

#include <type_traits>

#include "caf/sec.hpp"
#include "caf/catch_all.hpp"
#include "caf/message_id.hpp"
#include "caf/typed_behavior.hpp"
#include "caf/system_messages.hpp"

#include "caf/detail/type_list.hpp"
#include "caf/detail/typed_actor_util.hpp"

namespace caf {

/// This helper class identifies an expected response message
/// and enables `request(...).then(...)`.
template <class Self, class Output, bool IsBlocking>
class response_handle;

/******************************************************************************
 *                                 nonblocking                                *
 ******************************************************************************/
template <class Self, class Output>
class response_handle<Self, Output, false> {
public:
  response_handle() = delete;
  response_handle(const response_handle&) = default;
  response_handle& operator=(const response_handle&) = default;

  response_handle(message_id mid, Self* self) : mid_(mid), self_(self) {
    // nop
  }

  template <class F, class E = detail::is_callable_t<F>>
  void await(F f) const {
    await_impl(f);
  }

  template <class F, class OnError,
            class E1 = detail::is_callable_t<F>,
            class E2 = detail::is_handler_for_ef<OnError, error>>
  void await(F f, OnError e) const {
    await_impl(f, e);
  }

  template <class F, class E = detail::is_callable_t<F>>
  void then(F f) const {
    then_impl(f);
  }

  template <class F, class OnError,
            class E1 = detail::is_callable_t<F>,
            class E2 = detail::is_handler_for_ef<OnError, error>>
  void then(F f, OnError e) const {
    then_impl(f, e);
  }

private:
  template <class F>
  void await_impl(F& f) const {
    static_assert(std::is_same<
                    void,
                    typename detail::get_callable_trait<F>::result_type
                  >::value,
                  "response handlers are not allowed to have a return "
                  "type other than void");
    detail::type_checker<Output, F>::check();
    self_->add_awaited_response_handler(mid_, message_handler{std::move(f)});
  }

  template <class F, class OnError>
  void await_impl(F& f, OnError& ef) const {
    static_assert(std::is_same<
                    void,
                    typename detail::get_callable_trait<F>::result_type
                  >::value,
                  "response handlers are not allowed to have a return "
                  "type other than void");
    detail::type_checker<Output, F>::check();
    self_->add_awaited_response_handler(mid_, behavior{std::move(f),
                                                       std::move(ef)});
  }

  template <class F>
  void then_impl(F& f) const {
    static_assert(std::is_same<
                    void,
                    typename detail::get_callable_trait<F>::result_type
                  >::value,
                  "response handlers are not allowed to have a return "
                  "type other than void");
    detail::type_checker<Output, F>::check();
    self_->add_multiplexed_response_handler(mid_, behavior{std::move(f)});
  }

  template <class F, class OnError>
  void then_impl(F& f, OnError& ef) const {
    static_assert(std::is_same<
                    void,
                    typename detail::get_callable_trait<F>::result_type
                  >::value,
                  "response handlers are not allowed to have a return "
                  "type other than void");
    detail::type_checker<Output, F>::check();
    self_->add_multiplexed_response_handler(mid_, behavior{std::move(f),
                                                           std::move(ef)});
  }

  message_id mid_;
  Self* self_;
};

/******************************************************************************
 *                                  blocking                                  *
 ******************************************************************************/
template <class Self, class Output>
class response_handle<Self, Output, true> {
public:
  response_handle() = delete;
  response_handle(const response_handle&) = default;
  response_handle& operator=(const response_handle&) = default;

  response_handle(message_id mid, Self* self) : mid_(mid), self_(self) {
    // nop
  }

  using error_handler = std::function<void (error&)>;

  template <class F, class OnError,
            class E = detail::is_handler_for_ef<OnError, error>>
  detail::is_callable_t<F> receive(F f, OnError ef) {
    static_assert(std::is_same<
                    void,
                    typename detail::get_callable_trait<F>::result_type
                  >::value,
                  "response handlers are not allowed to have a return "
                  "type other than void");
    detail::type_checker<Output, F>::check();
    typename Self::accept_one_cond rc;
    self_->varargs_receive(rc, mid_, std::move(f), std::move(ef));
  }

  template <class OnError, class F,
            class E = detail::is_callable_t<F>>
  detail::is_handler_for_ef<OnError, error> receive(OnError ef, F f) {
    receive(std::move(f), std::move(ef));
  }

  template <class OnError, class F,
            class E = detail::is_handler_for_ef<OnError, error>>
  void receive(OnError ef, catch_all<F> ca) {
    typename Self::accept_one_cond rc;
    self_->varargs_receive(rc, mid_, std::move(ef), std::move(ca));
  }


private:
  message_id mid_;
  Self* self_;
};

} // namespace caf

#endif // CAF_RESPONSE_HANDLE_HPP
