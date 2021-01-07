// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <tuple>

#include "caf/detail/apply_args.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/int_list.hpp"
#include "caf/detail/overload.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/fwd.hpp"
#include "caf/make_sink_result.hpp"
#include "caf/make_source_result.hpp"
#include "caf/make_stage_result.hpp"
#include "caf/message.hpp"
#include "caf/none.hpp"
#include "caf/optional.hpp"
#include "caf/result.hpp"
#include "caf/skip.hpp"
#include "caf/unit.hpp"

namespace caf::detail {

/// Inspects the result of message handlers and triggers type-depended actions
/// such as generating result messages.
class CAF_CORE_EXPORT invoke_result_visitor {
public:
  virtual ~invoke_result_visitor();

  constexpr invoke_result_visitor() {
    // nop
  }

  // -- virtual handlers -------------------------------------------------------

  /// Called if the message handler returned an error.
  virtual void operator()(error&) = 0;

  /// Called if the message handler returned any "ordinary" value.
  virtual void operator()(message&) = 0;

  // -- extraction and conversions ---------------------------------------------

  /// Wraps arbitrary values into a `message` and calls the visitor recursively.
  template <class... Ts>
  void operator()(Ts&... xs) {
    static_assert(detail::conjunction<!detail::is_stream<Ts>::value...>::value,
                  "returning a stream<T> from a message handler achieves not "
                  "what you would expect and is most likely a mistake");
    auto tmp = make_message(std::move(xs)...);
    (*this)(tmp);
  }

  /// Called if the message handler returns `void` or `unit_t`.
  void operator()(const unit_t&) {
    message empty_msg;
    (*this)(empty_msg);
  }

  /// Disambiguates the variadic `operator<Ts...>()`.
  void operator()(unit_t& x) {
    (*this)(const_cast<const unit_t&>(x));
  }

  /// Dispatches on the runtime-type of `x`.
  template <class... Ts>
  void operator()(result<Ts...>& res) {
    caf::visit([this](auto& x) { (*this)(x); }, res);
  }

  // -- special-purpose handlers that don't produce results --------------------

  void operator()(response_promise&) {
    // nop
  }

  template <class... Ts>
  void operator()(typed_response_promise<Ts...>&) {
    // nop
  }

  template <class... Ts>
  void operator()(delegated<Ts...>&) {
    // nop
  }

  template <class Out, class... Ts>
  void operator()(outbound_stream_slot<Out, Ts...>&) {
    // nop
  }

  template <class In>
  void operator()(inbound_stream_slot<In>&) {
    // nop
  }

  template <class In>
  void operator()(make_sink_result<In>&) {
    // nop
  }

  template <class DownstreamManager, class... Ts>
  void operator()(make_source_result<DownstreamManager, Ts...>&) {
    // nop
  }

  template <class In, class DownstreamManager, class... Ts>
  void operator()(make_stage_result<In, DownstreamManager, Ts...>&) {
    // nop
  }
};

} // namespace caf::detail
