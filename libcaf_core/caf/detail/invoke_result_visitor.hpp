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

#include <tuple>

#include "caf/expected.hpp"
#include "caf/fwd.hpp"
#include "caf/make_message.hpp"
#include "caf/make_sink_result.hpp"
#include "caf/make_source_result.hpp"
#include "caf/make_stage_result.hpp"
#include "caf/message.hpp"
#include "caf/none.hpp"
#include "caf/optional.hpp"
#include "caf/result.hpp"
#include "caf/skip.hpp"
#include "caf/unit.hpp"

#include "caf/detail/apply_args.hpp"
#include "caf/detail/int_list.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf {
namespace detail {

/// Inspects the result of message handlers and triggers type-depended actions
/// such as generating result messages.
class invoke_result_visitor {
public:
  virtual ~invoke_result_visitor();

  constexpr invoke_result_visitor() {
    // nop
  }

  // -- virtual handlers -------------------------------------------------------

  /// Called whenever no result messages gets produced, e.g., when returning a
  /// `response_promise`.
  virtual void operator()() = 0;

  /// Called if the message handler returned an error.
  virtual void operator()(error&) = 0;

  /// Called if the message handler returned any "ordinary" value.
  virtual void operator()(message&) = 0;

  /// Called if the message handler returns "nothing", for example a
  /// default-constructed `optional<T>`.
  virtual void operator()(const none_t&) = 0;

  // -- on-the-fly type conversions --------------------------------------------

  /// Called if the message handler returns `void` or `unit_t`.
  inline void operator()(const unit_t&) {
    message empty_msg;
    (*this)(empty_msg);
  }

  /// Unwraps an `optional<T>` by recursively calling the visitor with either
  /// `none_t` or `T`.
  template <class T>
  void operator()(optional<T>& x) {
    if (x)
      (*this)(*x);
    else
      (*this)(none);
  }

  /// Unwraps an `expected<T>` by recursively calling the visitor with either
  /// `error` or `T`.
  template <class T>
  void operator()(expected<T>& x) {
    if (x)
      (*this)(*x);
    else
      (*this)(x.error());
  }

  /// Wraps arbitrary values into a `message` and calls the visitor recursively.
  template <class... Ts>
  void operator()(Ts&... xs) {
    static_assert(detail::conjunction<!detail::is_stream<Ts>::value...>::value,
                  "returning a stream<T> from a message handler achieves not "
                  "what you would expect and is most likely a mistake");
    auto tmp = make_message(std::move(xs)...);
    (*this)(tmp);
  }

  /// Wraps the tuple into a `message` and calls the visitor recursively with
  /// its contents.
  template <class... Ts>
  void operator()(std::tuple<Ts...>& xs) {
    apply_args(*this, get_indices(xs), xs);
  }

  /// Disambiguates the variadic `operator<Ts...>()`.
  inline void operator()(none_t& x) {
    (*this)(const_cast<const none_t&>(x));
  }

  /// Disambiguates the variadic `operator<Ts...>()`.
  inline void operator()(unit_t& x) {
    (*this)(const_cast<const unit_t&>(x));
  }

  // -- special-purpose handlers that don't procude results --------------------

  /// Calls `(*this)()`.
  inline void operator()(response_promise&) {
    (*this)();
  }

  /// Calls `(*this)()`.
  template <class... Ts>
  void operator()(typed_response_promise<Ts...>&) {
    (*this)();
  }

  /// Calls `(*this)()`.
  template <class... Ts>
  void operator()(delegated<Ts...>&) {
    (*this)();
  }

  /// Calls `(*this)()`.
  template <class Out, class... Ts>
  void operator()(output_stream<Out, Ts...>&) {
    (*this)();
  }

  /// Calls `(*this)()`.
  template <class Out, class... Ts>
  void operator()(outbound_stream_slot<Out, Ts...>&) {
    (*this)();
  }

  /// Calls `(*this)()`.
  template <class In>
  void operator()(inbound_stream_slot<In>&) {
    (*this)();
  }

  /// Calls `(*this)()`.
  template <class In>
  void operator()(make_sink_result<In>&) {
    (*this)();
  }

  /// Calls `(*this)()`.
  template <class DownstreamManager, class... Ts>
  void operator()(make_source_result<DownstreamManager, Ts...>&) {
    (*this)();
  }

  /// Calls `(*this)()`.
  template <class In, class DownstreamManager, class... Ts>
  void operator()(make_stage_result<In, DownstreamManager, Ts...>&) {
    (*this)();
  }

  // -- visit API: return true if T was visited, false if T was skipped --------

  /// Delegates `x` to the appropriate handler and returns `true`.
  template <class T>
  bool visit(T& x) {
    (*this)(x);
    return true;
  }

  /// Returns `false`.
  inline bool visit(skip_t&) {
    return false;
  }

  /// Returns `false`.
  inline bool visit(const skip_t&) {
    return false;
  }

  /// Returns `false` if `x != none`, otherwise calls the void handler and
  /// returns `true`..
  inline bool visit(optional<skip_t>& x) {
    if (x)
      return false;
    (*this)();
    return true;
  }

  /// Dispatches on the runtime-type of `x`.
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

