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

#ifndef CAF_DETAIL_INVOKE_VISITOR_HPP
#define CAF_DETAIL_INVOKE_VISITOR_HPP

#include <tuple>

#include "caf/fwd.hpp"
#include "caf/none.hpp"
#include "caf/unit.hpp"
#include "caf/skip.hpp"
#include "caf/result.hpp"
#include "caf/message.hpp"
#include "caf/expected.hpp"
#include "caf/optional.hpp"
#include "caf/make_message.hpp"

#include "caf/detail/int_list.hpp"
#include "caf/detail/apply_args.hpp"

namespace caf {
namespace detail {

class invoke_result_visitor {
public:
  virtual ~invoke_result_visitor();

  constexpr invoke_result_visitor() {
    // nop
  }

  // severs as catch-all case for all values producing
  // no result such as response_promise
  virtual void operator()() = 0;

  // called if the message handler explicitly returned an error
  virtual void operator()(error&) = 0;

  // called if the message handler returned any "ordinary" value
  virtual void operator()(message&) = 0;

  // called if the message handler returns explictly none or unit
  virtual void operator()(const none_t&) = 0;

  // map unit to an empty message
  inline void operator()(const unit_t&) {
    message empty_msg;
    (*this)(empty_msg);
  }

  // unwrap optionals
  template <class T>
  void operator()(optional<T>& x) {
    if (x)
      (*this)(*x);
    else
      (*this)(none);
  }

  // unwrap expecteds
  template <class T>
  void operator()(expected<T>& x) {
    if (x)
      (*this)(*x);
    else
      (*this)(x.error());
  }

  // convert values to messages
  template <class... Ts>
  void operator()(Ts&... xs) {
    auto tmp = make_message(std::move(xs)...);
    (*this)(tmp);
  }

  // unwrap tuples
  template <class... Ts>
  void operator()(std::tuple<Ts...>& xs) {
    apply_args(*this, get_indices(xs), xs);
  }

  // disambiguations

  inline void operator()(none_t& x) {
    (*this)(const_cast<const none_t&>(x));
  }

  inline void operator()(unit_t& x) {
    (*this)(const_cast<const unit_t&>(x));
  }

  // special purpose handler that don't procude results

  inline void operator()(response_promise&) {
    (*this)();
  }

  template <class... Ts>
  void operator()(typed_response_promise<Ts...>&) {
    (*this)();
  }

  template <class... Ts>
  void operator()(delegated<Ts...>&) {
    (*this)();
  }

  // visit API - returns true if T was visited, false if T was skipped

  template <class T>
  bool visit(T& x) {
    (*this)(x);
    return true;
  }

  inline bool visit(skip_t&) {
    return false;
  }

  inline bool visit(const skip_t&) {
    return false;
  }

  inline bool visit(optional<skip_t>& x) {
    if (x)
      return false;
    (*this)();
    return true;
  }

  template <class... Ts>
  bool visit(result<Ts...>& x) {
    switch (x.flag) {
      case rt_value:
        (*this)(x.value);
        return true;
      case rt_error:
        (*this)(x.err);
        return true;
      case rt_delegated:
        (*this)();
        return true;
      default:
        return false;
    }
  }
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_INVOKE_VISITOR_HPP
