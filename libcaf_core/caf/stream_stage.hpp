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

#ifndef CAF_DETAIL_STREAM_STAGE_HPP
#define CAF_DETAIL_STREAM_STAGE_HPP

#include <tuple>

#include "caf/intrusive_ptr.hpp"
#include "caf/stream_sink.hpp"
#include "caf/stream_source.hpp"

namespace caf {

template <class In, class Result, class Out, class Scatterer,
          class... HandshakeData>
class stream_stage : public stream_source<Out, Scatterer, HandshakeData...>,
                     public stream_sink<In, Result> {
public:
  // -- member types -----------------------------------------------------------

  using left_super = stream_source<Out, Scatterer, HandshakeData...>;

  using right_super = stream_sink<In, Result>;

  // -- constructors, destructors, and assignment operators --------------------

  stream_stage(local_actor* self)
      : stream_manager(self),
        left_super(self),
        right_super(self) {
    // nop
  }
};

template <class In, class Result, class Out, class Scatterer,
          class... HandshakeData>
using stream_stage_ptr =
  intrusive_ptr<stream_stage<In, Result, Out, Scatterer, HandshakeData...>>;

} // namespace caf

#endif // CAF_DETAIL_STREAM_STAGE_HPP
