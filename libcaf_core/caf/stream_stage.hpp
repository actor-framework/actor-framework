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

#ifndef CAF_STREAM_STAGE_HPP
#define CAF_STREAM_STAGE_HPP

#include "caf/fwd.hpp"
#include "caf/extend.hpp"
#include "caf/stream_handler.hpp"

#include "caf/mixin/has_upstreams.hpp"
#include "caf/mixin/has_downstreams.hpp"

namespace caf {

/// Models a stream stage with up- and downstreams.
class stream_stage : public extend<stream_handler, stream_stage>::
                            with<mixin::has_upstreams, mixin::has_downstreams> {
public:
  // -- constructors, destructors, and assignment operators --------------------

  stream_stage(abstract_upstream* in_ptr, abstract_downstream* out_ptr);

  // -- overrides --------------------------------------------------------------

  bool done() const override;

  void abort(strong_actor_ptr&, const error&) override;

  error downstream_demand(strong_actor_ptr&, size_t) override;

  error upstream_batch(strong_actor_ptr&, size_t, message&) override;

  void last_upstream_closed();

  inline abstract_upstream& in() {
    return *in_ptr_;
  }

  inline abstract_downstream& out() {
    return *out_ptr_;
  }

protected:
  virtual error process_batch(message& msg) = 0;

private:
  abstract_upstream* in_ptr_;
  abstract_downstream* out_ptr_;
};

} // namespace caf

#endif // CAF_STREAM_STAGE_HPP
