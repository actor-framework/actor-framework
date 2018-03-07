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
 * http://opensink.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_DETAIL_STREAM_SINK_HPP
#define CAF_DETAIL_STREAM_SINK_HPP

#include <tuple>

#include "caf/intrusive_ptr.hpp"
#include "caf/stream_manager.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {

template <class In, class Result>
class stream_sink : public virtual stream_manager {
public:
  // -- member types -----------------------------------------------------------

  using input_type = In;

  // -- constructors, destructors, and assignment operators --------------------

  stream_sink(scheduled_actor* self) : stream_manager(self) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  /// Creates a new input path to the current sender.
  make_sink_result<In, Result> add_inbound_path(const stream<In>& in);
};

template <class In, class Result>
using stream_sink_ptr = intrusive_ptr<stream_sink<In, Result>>;

} // namespace caf

#include "caf/make_sink_result.hpp"

namespace caf {

template <class In, class Result>
make_sink_result<In, Result>
stream_sink<In, Result>::add_inbound_path(const stream<In>&) {
  return {this->assign_next_slot(), this};
}

} // namespace caf

#endif // CAF_DETAIL_STREAM_SINK_HPP
