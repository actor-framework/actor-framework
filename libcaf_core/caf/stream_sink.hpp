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

#ifndef CAF_STREAM_SINK_HPP
#define CAF_STREAM_SINK_HPP

#include <vector>

#include "caf/extend.hpp"
#include "caf/message_id.hpp"
#include "caf/stream_handler.hpp"
#include "caf/upstream_policy.hpp"
#include "caf/actor_control_block.hpp"

#include "caf/mixin/has_upstreams.hpp"

namespace caf {

/// Represents a terminal stream stage.
class stream_sink : public extend<stream_handler, stream_sink>::
                           with<mixin::has_upstreams> {
public:
  stream_sink(abstract_upstream* in_ptr, strong_actor_ptr&& orig_sender,
              std::vector<strong_actor_ptr>&& trailing_stages, message_id mid);

  bool done() const override;

  error upstream_batch(strong_actor_ptr& src, long xs_size,
                       message& xs) override;

  void abort(strong_actor_ptr& cause, const error& reason) override;

  void last_upstream_closed();

  inline abstract_upstream& in() {
    return *in_ptr_;
  }

  long min_buffer_size() const {
    return min_buffer_size_;
  }

protected:
  /// Consumes a batch.
  virtual error consume(message& xs) = 0;

  /// Computes the final result after consuming all batches of the stream.
  virtual message finalize() = 0;

private:
  abstract_upstream* in_ptr_;
  strong_actor_ptr original_sender_;
  std::vector<strong_actor_ptr> next_stages_;
  message_id original_msg_id_;
  long min_buffer_size_;
};

} // namespace caf

#endif // CAF_STREAM_SINK_HPP
