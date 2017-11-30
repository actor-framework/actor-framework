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

#ifndef CAF_STREAM_GATHERER_IMPL_HPP
#define CAF_STREAM_GATHERER_IMPL_HPP

#include <vector>
#include <cstdint>
#include <utility>

#include "caf/fwd.hpp"
#include "caf/stream_gatherer.hpp"
#include "caf/stream_edge_impl.hpp"
#include "caf/response_promise.hpp"

namespace caf {

/// Type-erased policy for receiving data from sources.
class stream_gatherer_impl : public stream_edge_impl<stream_gatherer> {
public:
  using super = stream_edge_impl<stream_gatherer>;

  using assignment_pair = std::pair<path_type*, long>;

  stream_gatherer_impl(local_actor* selfptr);

  ~stream_gatherer_impl();

  path_ptr add_path(stream_slot slot, strong_actor_ptr x,
                    strong_actor_ptr original_stage, stream_priority prio,
                    long available_credit, bool redeployable,
                    response_promise result_cb) override;

  bool remove_path(stream_slot slot, const actor_addr& x, error reason,
                   bool silent) override;

  void close(message result) override;

  void abort(error reason) override;

protected:
  void emit_credits();

  std::vector<assignment_pair> assignment_vec_;

  /// Listeners for the final result.
  std::vector<response_promise> listeners_;
};

} // namespace caf

#endif // CAF_STREAM_GATHERER_IMPL_HPP
