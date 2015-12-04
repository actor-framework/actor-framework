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

#ifndef CAF_RESPONSE_HANDLE_HPP
#define CAF_RESPONSE_HANDLE_HPP

#include <type_traits>

#include "caf/sec.hpp"
#include "caf/message_id.hpp"
#include "caf/typed_behavior.hpp"
#include "caf/continue_helper.hpp"
#include "caf/system_messages.hpp"
#include "caf/typed_continue_helper.hpp"

#include "caf/detail/type_list.hpp"
#include "caf/detail/typed_actor_util.hpp"

namespace caf {

/// This tag identifies response handles featuring a
/// nonblocking API by providing a `then` member function.
/// @relates response_handle
struct nonblocking_response_handle_tag {};

/// This tag identifies response handles featuring a
/// blocking API by providing an `await` member function.
/// @relates response_handle
struct blocking_response_handle_tag {};

/// This helper class identifies an expected response message
/// and enables `request(...).then(...)`.
template <class Self, class Output, class Tag>
class response_handle;

template <class Output, class F>
struct get_continue_helper {
  using type = typed_continue_helper<
                 typename detail::lifted_result_type<
                   typename detail::get_callable_trait<F>::result_type
                 >::type
               >;
};

template <class F>
struct get_continue_helper<message, F> {
  using type = continue_helper;
};

/******************************************************************************
 *                                 nonblocking                                *
 ******************************************************************************/
template <class Self, class Output>
class response_handle<Self, Output, nonblocking_response_handle_tag> {
public:
  response_handle() = delete;
  response_handle(const response_handle&) = default;
  response_handle& operator=(const response_handle&) = default;

  response_handle(message_id mid, Self* self) : mid_(mid), self_(self) {
    // nop
  }

  using error_handler = std::function<void (error&)>;

  template <class F, class T>
  typename get_continue_helper<Output, F>::type
  then(F f, error_handler ef, timeout_definition<T> tdef) const {
    return then_impl(f, ef, std::move(tdef));
  }

  template <class F>
  typename get_continue_helper<Output, F>::type
  then(F f, error_handler ef = nullptr) const {
    return then_impl(f, ef);
  }

  template <class F, class T>
  typename get_continue_helper<Output, F>::type
  then(F f, timeout_definition<T> tdef) const {
    return then(std::move(f), nullptr, std::move(tdef));
  }

  void generic_then(std::function<void (message&)> f, error_handler ef) {
    behavior tmp{
      others >> f
    };
    self_->set_response_handler(mid_, behavior{std::move(tmp)}, std::move(ef));
  }

private:
  template <class F, class... Ts>
  typename get_continue_helper<Output, F>::type
  then_impl(F& f, error_handler& ef, Ts&&... xs) const {
    static_assert(detail::is_callable<F>::value, "argument is not callable");
    static_assert(! std::is_base_of<match_case, F>::value,
                  "match cases are not allowed in this context");
    detail::type_checker<Output, F>::check();
    self_->set_response_handler(mid_,
                                behavior{std::move(f), std::forward<Ts>(xs)...},
                                std::move(ef));
    return {mid_};
  }


  message_id mid_;
  Self* self_;
};

/******************************************************************************
 *                                  blocking                                  *
 ******************************************************************************/
template <class Self, class Output>
class response_handle<Self, Output, blocking_response_handle_tag> {
public:
  response_handle() = delete;
  response_handle(const response_handle&) = default;
  response_handle& operator=(const response_handle&) = default;

  response_handle(message_id mid, Self* self) : mid_(mid), self_(self) {
    // nop
  }

  using error_handler = std::function<void (error&)>;

  template <class F, class T>
  void await(F f, error_handler ef, timeout_definition<T> tdef) {
    await_impl(f, ef, std::move(tdef));
  }

  template <class F>
  void await(F f, error_handler ef = nullptr) {
    await_impl(f, ef);
  }

  template <class F, class T>
  void await(F f, timeout_definition<T> tdef) {
    await(std::move(f), nullptr, std::move(tdef));
  }

private:
  template <class F, class... Ts>
  void await_impl(F& f, error_handler& ef, Ts&&... xs) {
    static_assert(detail::is_callable<F>::value, "argument is not callable");
    static_assert(! std::is_base_of<match_case, F>::value,
                  "match cases are not allowed in this context");
    detail::type_checker<Output, F>::check();
    behavior tmp;
    if (! ef)
      tmp.assign(
        std::move(f),
        others >> [=] {
          self_->quit(exit_reason::unhandled_sync_failure);
        },
        std::forward<Ts>(xs)...
      );
    else
      tmp.assign(
        std::move(f),
        ef,
        others >> [ef] {
          error err = sec::unexpected_response;
          ef(err);
        },
        std::forward<Ts>(xs)...
      );
    self_->dequeue(tmp, mid_);
  }

  message_id mid_;
  Self* self_;
};

} // namespace caf

#endif // CAF_RESPONSE_HANDLE_HPP
