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

  path_ptr add_path(const stream_id& sid, strong_actor_ptr x,
                    strong_actor_ptr original_stage, stream_priority prio,
                    long available_credit, bool redeployable,
                    response_promise result_cb) override;

  bool remove_path(const stream_id& sid, const actor_addr& x, error reason,
                   bool silent) override;

  void close(message result) override;

  void abort(error reason) override;

  long high_watermark() const override;

  long min_credit_assignment() const override;

  long max_credit() const override;

  void high_watermark(long x) override;

  void min_credit_assignment(long x) override;

  void max_credit(long x) override;

protected:
  void emit_credits();

  long high_watermark_;
  long min_credit_assignment_;
  long max_credit_;
  std::vector<assignment_pair> assignment_vec_;

  /// Listeners for the final result.
  std::vector<response_promise> listeners_;
};

} // namespace caf

#endif // CAF_STREAM_GATHERER_IMPL_HPP
