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
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_RESPONSE_HANDLE_HPP
#define CAF_RESPONSE_HANDLE_HPP

#include <type_traits>

#include "caf/message_id.hpp"
#include "caf/typed_behavior.hpp"
#include "caf/continue_helper.hpp"
#include "caf/typed_continue_helper.hpp"

#include "caf/detail/type_list.hpp"

#include "caf/detail/typed_actor_util.hpp"
#include "caf/detail/response_handle_util.hpp"

namespace caf {

/**
 * This tag identifies response handles featuring a
 * nonblocking API by providing a `then` member function.
 * @relates response_handle
 */
struct nonblocking_response_handle_tag {};

/**
 * This tag identifies response handles featuring a
 * blocking API by providing an `await` member function.
 * @relates response_handle
 */
struct blocking_response_handle_tag {};

/**
 * This helper class identifies an expected response message
 * and enables `sync_send(...).then(...)`.
 */
template <class Self, class Result, class Tag>
class response_handle;

/******************************************************************************
 *                            nonblocking + untyped                           *
 ******************************************************************************/
template <class Self>
class response_handle<Self, message, nonblocking_response_handle_tag> {
 public:
  response_handle() = delete;
  response_handle(const response_handle&) = default;
  response_handle& operator=(const response_handle&) = default;

  response_handle(message_id mid, Self* self) : m_mid(mid), m_self(self) {
    // nop
  }

  continue_helper then(behavior bhvr) const {
    m_self->bhvr_stack().push_back(std::move(bhvr), m_mid);
    auto ptr = m_self;
    return {m_mid, [ptr](message_id mid) { return ptr->sync_handler(mid); }};
  }

  template <class... Cs, class... Ts>
  continue_helper then(const match_expr<Cs...>& arg,
             const Ts&... args) const {
    return then(behavior{arg, args...});
  }

  template <class... Fs>
  typename std::enable_if<
    detail::all_callable<Fs...>::value,
    continue_helper
  >::type
  then(Fs... fs) const {
    return then(detail::fs2bhvr(m_self, fs...));
  }

 private:
  message_id m_mid;
  Self* m_self;
};

/******************************************************************************
 *                            nonblocking + typed                             *
 ******************************************************************************/
template <class Self, class... Ts>
class response_handle<Self, detail::type_list<Ts...>,
                      nonblocking_response_handle_tag> {
 public:
  response_handle() = delete;
  response_handle(const response_handle&) = default;
  response_handle& operator=(const response_handle&) = default;

  response_handle(message_id mid, Self* self) : m_mid(mid), m_self(self) {
    // nop
  }

  template <class F,
            class Enable = typename std::enable_if<
                             detail::is_callable<F>::value
                             && !is_match_expr<F>::value
                           >::type>
  typed_continue_helper<
    typename detail::lifted_result_type<
      typename detail::get_callable_trait<F>::result_type
    >::type>
  then(F fun) {
    detail::assert_types<detail::type_list<Ts...>, F>();
    m_self->bhvr_stack().push_back(behavior{on_arg_match >> fun}, m_mid);
    auto ptr = m_self;
    return {m_mid, [ptr](message_id mid) { return ptr->sync_handler(mid); }};
  }

 private:
  message_id m_mid;
  Self* m_self;
};

/******************************************************************************
 *                             blocking + untyped                             *
 ******************************************************************************/
template <class Self>
class response_handle<Self, message, blocking_response_handle_tag> {
 public:
  response_handle() = delete;
  response_handle(const response_handle&) = default;
  response_handle& operator=(const response_handle&) = default;

  response_handle(message_id mid, Self* self) : m_mid(mid), m_self(self) {
    // nop
  }

  void await(behavior& pfun) {
    m_self->dequeue_response(pfun, m_mid);
  }

  void await(behavior&& pfun) {
    behavior tmp{std::move(pfun)};
    await(tmp);
  }

  template <class... Cs, class... Ts>
  void await(const match_expr<Cs...>& arg, const Ts&... args) {
    behavior bhvr{arg, args...};
    await(bhvr);
  }

  template <class... Fs>
  typename std::enable_if<detail::all_callable<Fs...>::value>::type
  await(Fs... fs) {
    await(detail::fs2bhvr(m_self, fs...));
  }

 private:
  message_id m_mid;
  Self* m_self;
};

/******************************************************************************
 *                              blocking + typed                              *
 ******************************************************************************/
template <class Self, class... Ts>
class response_handle<Self, detail::type_list<Ts...>,
                      blocking_response_handle_tag> {
 public:
  using result_types = detail::type_list<Ts...>;

  response_handle() = delete;
  response_handle(const response_handle&) = default;
  response_handle& operator=(const response_handle&) = default;

  response_handle(message_id mid, Self* self) : m_mid(mid), m_self(self) {
    // nop
  }

  template <class F>
  void await(F fun) {
    using arg_types =
      typename detail::tl_map<
        typename detail::get_callable_trait<F>::arg_types,
        detail::rm_const_and_ref
      >::type;
    static constexpr size_t fun_args = detail::tl_size<arg_types>::value;
    static_assert(fun_args <= detail::tl_size<result_types>::value,
            "functor takes too much arguments");
    using recv_types =
      typename detail::tl_right<
        result_types,
        fun_args
      >::type;
    static_assert(std::is_same<arg_types, recv_types>::value,
            "wrong functor signature");
    behavior tmp = detail::fs2bhvr(m_self, fun);
    m_self->dequeue_response(tmp, m_mid);
  }

 private:
  message_id m_mid;
  Self* m_self;
};

} // namespace caf

#endif // CAF_RESPONSE_HANDLE_HPP
