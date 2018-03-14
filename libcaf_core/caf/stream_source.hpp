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

#ifndef CAF_DETAIL_STREAM_SOURCE_HPP
#define CAF_DETAIL_STREAM_SOURCE_HPP

#include <tuple>

#include "caf/fwd.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/logger.hpp"
#include "caf/stream_manager.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {

template <class Out, class DownstreamManager>
class stream_source : public virtual stream_manager {
public:
  // -- member types -----------------------------------------------------------

  using output_type = Out;

  // -- constructors, destructors, and assignment operators --------------------

  stream_source(scheduled_actor* self) : stream_manager(self), out_(self) {
    // nop
  }

  DownstreamManager& out() override {
    return out_;
  }

  /// Creates a new output path to the current sender.
  output_stream<Out, std::tuple<>, intrusive_ptr<stream_source>>
  add_outbound_path() {
    CAF_LOG_TRACE("");
    return this->add_unchecked_outbound_path<Out>().rebind(this);
  }

  /// Creates a new output path to the current sender with custom handshake.
  template <class... Ts>
  output_stream<Out, std::tuple<detail::strip_and_convert_t<Ts>...>,
                intrusive_ptr<stream_source>>
  add_outbound_path(std::tuple<Ts...> xs) {
    CAF_LOG_TRACE(CAF_ARG(xs));
    return this->add_unchecked_outbound_path<Out>(std::move(xs)).rebind(this);
  }

  /// Creates a new output path to the current sender.
  template <class Handle>
  output_stream<Out, std::tuple<>, intrusive_ptr<stream_source>>
  add_outbound_path(const Handle& next) {
    CAF_LOG_TRACE(CAF_ARG(next));
    return this->add_unchecked_outbound_path<Out>(next).rebind(this);
  }

  /// Creates a new output path to the current sender with custom handshake.
  template <class Handle, class... Ts>
  output_stream<Out, std::tuple<detail::strip_and_convert_t<Ts>...>,
                intrusive_ptr<stream_source>>
  add_outbound_path(const Handle& next, std::tuple<Ts...> xs) {
    CAF_LOG_TRACE(CAF_ARG(next) << CAF_ARG(xs));
    return this->add_unchecked_outbound_path<Out>(next, std::move(xs))
           .rebind(this);
  }

protected:
  DownstreamManager out_;
};

template <class Out, class DownstreamManager>
using stream_source_ptr = intrusive_ptr<stream_source<Out, DownstreamManager>>;

template <class DownstreamManager>
using stream_source_ptr_t =
  stream_source_ptr<typename DownstreamManager::output_type, DownstreamManager>;

} // namespace caf

#endif // CAF_DETAIL_STREAM_SOURCE_HPP
