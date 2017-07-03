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

#ifndef CAF_DETAIL_STREAM_SERV_DOWNSTREAM_HPP
#define CAF_DETAIL_STREAM_SERV_DOWNSTREAM_HPP

#include <deque>
#include <cstdint>
#include <algorithm>
#include <unordered_map>

#include "caf/actor.hpp"
#include "caf/error.hpp"
#include "caf/stream_msg.hpp"
#include "caf/local_actor.hpp"

#include "caf/detail/stream_multiplexer.hpp"

namespace caf {
namespace detail {

// Forwards messages from local actors to a remote stream_serv.
class outgoing_stream_multiplexer : public stream_multiplexer {
public:
  /// Allow `variant` to recognize this type as a visitor.
  using result_type = void;

  outgoing_stream_multiplexer(local_actor* self, backend& service);

  void operator()(stream_msg& x);

  /// @pre `self_->current_sender() != nullptr && current_stream_state_valid()`
  void operator()(stream_msg::open&);

  /// @pre `self_->current_sender() != nullptr && current_stream_state_valid()`
  void operator()(stream_msg::ack_open&);

  /// @pre `self_->current_sender() != nullptr && current_stream_state_valid()`
  void operator()(stream_msg::batch&);

  /// @pre `self_->current_sender() != nullptr && current_stream_state_valid()`
  void operator()(stream_msg::ack_batch&);

  /// @pre `self_->current_sender() != nullptr && current_stream_state_valid()`
  void operator()(stream_msg::close&);

  /// @pre `self_->current_sender() != nullptr && current_stream_state_valid()`
  void operator()(stream_msg::abort&);

  /// @pre `self_->current_sender() != nullptr && current_stream_state_valid()`
  void operator()(stream_msg::downstream_failed&);

  /// @pre `self_->current_sender() != nullptr && current_stream_state_valid()`
  void operator()(stream_msg::upstream_failed&);

private:
  // Forwards the current stream_msg upstream. 
  // @pre `current_stream_msg != nullptr`
  void forward_to_upstream();

  // Forwards the current stream_msg downstream. 
  // @pre `current_stream_msg != nullptr`
  void forward_to_downstream();
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_STREAM_SERV_DOWNSTREAM_HPP
