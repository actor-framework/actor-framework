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
/// and enables `sync_send(...).then(...)`.
template <class Self, class ResultOptPairOrMessage, class Tag>
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

  response_handle(message_id mid, Self* self) : mid_(mid), self_(self) {
    // nop
  }

  template <class... Ts>
  continue_helper then(Ts&&... xs) const {
    self_->set_response_handler(mid_, behavior{std::forward<Ts>(xs)...});
    return {mid_};
  }

private:
  message_id mid_;
  Self* self_;
};

/******************************************************************************
 *                            nonblocking + typed                             *
 ******************************************************************************/
template <class Self, class TypedOutputPair>
class response_handle<Self, TypedOutputPair, nonblocking_response_handle_tag> {
public:
  response_handle() = delete;
  response_handle(const response_handle&) = default;
  response_handle& operator=(const response_handle&) = default;

  response_handle(message_id mid, Self* self) : mid_(mid), self_(self) {
    // nop
  }

  template <class... Fs>
  typed_continue_helper<
    typename detail::lifted_result_type<
      typename detail::common_result_type<
        typename detail::get_callable_trait<Fs>::result_type...
      >::type
    >::type>
  then(Fs... fs) {
    static_assert(sizeof...(Fs) > 0, "at least one functor is requried");
    static_assert(detail::conjunction<detail::is_callable<Fs>::value...>::value,
                  "all arguments must be callable");
    static_assert(detail::conjunction<
                    ! std::is_base_of<match_case, Fs>::value...
                  >::value,
                  "match cases are not allowed in this context");
    detail::type_checker<TypedOutputPair, Fs...>::check();
    self_->set_response_handler(mid_, behavior{std::move(fs)...});
    return {mid_};
  }

private:
  message_id mid_;
  Self* self_;
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

  response_handle(message_id mid, Self* self) : mid_(mid), self_(self) {
    // nop
  }

  void await(behavior& bhvr) {
    self_->dequeue(bhvr, mid_);
  }

  template <class... Ts>
  void await(Ts&&... xs) const {
    behavior bhvr{std::forward<Ts>(xs)...};
    self_->dequeue(bhvr, mid_);
  }

private:
  message_id mid_;
  Self* self_;
};

/******************************************************************************
 *                              blocking + typed                              *
 ******************************************************************************/
template <class Self, class OutputPair>
class response_handle<Self, OutputPair, blocking_response_handle_tag> {
public:
  response_handle() = delete;
  response_handle(const response_handle&) = default;
  response_handle& operator=(const response_handle&) = default;

  response_handle(message_id mid, Self* self) : mid_(mid), self_(self) {
    // nop
  }

  static constexpr bool is_either_or_handle =
    ! std::is_same<
      none_t,
      typename OutputPair::second
    >::value;

  template <class... Fs>
  void await(Fs... fs) {
    static_assert(sizeof...(Fs) > 0,
                  "at least one argument is required");
    static_assert((is_either_or_handle && sizeof...(Fs) == 2)
                  || sizeof...(Fs) == 1,
                  "wrong number of functors");
    static_assert(detail::conjunction<detail::is_callable<Fs>::value...>::value,
                  "all arguments must be callable");
    static_assert(detail::conjunction<
                    ! std::is_base_of<match_case, Fs>::value...
                  >::value,
                  "match cases are not allowed in this context");
    detail::type_checker<OutputPair, Fs...>::check();
    behavior tmp{std::move(fs)...};
    self_->dequeue(tmp, mid_);
  }

private:
  message_id mid_;
  Self* self_;
};

} // namespace caf

#endif // CAF_RESPONSE_HANDLE_HPP
