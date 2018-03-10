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
#include "caf/stream_result.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {

template <class In>
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
  stream_result<intrusive_ptr<stream_sink>>
  add_inbound_path(const stream<input_type>&) {
    return {add_unsafe_inbound_path_impl(), this};
  }
};

template <class In>
using stream_sink_ptr = intrusive_ptr<stream_sink<In>>;

} // namespace caf

#endif // CAF_DETAIL_STREAM_SINK_HPP
