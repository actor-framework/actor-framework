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

#ifndef CAF_STREAM_SCATTERER_IMPL_HPP
#define CAF_STREAM_SCATTERER_IMPL_HPP

#include <cstddef>

#include "caf/fwd.hpp"
#include "caf/duration.hpp"
#include "caf/stream_edge_impl.hpp"
#include "caf/stream_scatterer.hpp"

namespace caf {

/// Type-erased policy for dispatching data to sinks.
class stream_scatterer_impl : public stream_edge_impl<stream_scatterer> {
public:
  // -- member types -----------------------------------------------------------

  using super = stream_edge_impl<stream_scatterer>;

  // -- constructors, destructors, and assignment operators --------------------

  stream_scatterer_impl(local_actor* selfptr);

  ~stream_scatterer_impl() override;

  // -- convenience functions for children classes -----------------------------

  /// Removes all paths gracefully.
  void close() override;

  /// Removes all paths with an error message.
  void abort(error reason) override;

  /// Returns the total number (sum) of all credit on all paths.
  long total_credit() const;

  /// Returns the minimum number of credit on all paths.
  long min_credit() const;

  /// Returns the maximum number of credit on all paths.
  long max_credit() const;

  /// Folds `paths()` by extracting the `open_credit` from each path.
  long fold_credit(long x0, long (*f)(long, long)) const;

  // -- overridden functions ---------------------------------------------------

  path_ptr add_path(const stream_id& sid, strong_actor_ptr origin,
                    strong_actor_ptr sink_ptr,
                    mailbox_element::forwarding_stack stages,
                    message_id handshake_mid, message handshake_data,
                    stream_priority prio, bool redeployable) override;

  path_ptr confirm_path(const stream_id& sid, const actor_addr& from,
                        strong_actor_ptr to, long initial_demand,
                        bool redeployable) override;

  bool paths_clean() const override;

  long min_batch_size() const override;

  long max_batch_size() const override;

  long min_buffer_size() const override;

  duration max_batch_delay() const override;

  void min_batch_size(long x) override;

  void max_batch_size(long x) override;

  void min_buffer_size(long x) override;

  void max_batch_delay(duration x) override;

protected:
  long min_batch_size_;
  long max_batch_size_;
  long min_buffer_size_;
  duration max_batch_delay_;
};

} // namespace caf

#endif // CAF_STREAM_SCATTERER_IMPL_HPP
