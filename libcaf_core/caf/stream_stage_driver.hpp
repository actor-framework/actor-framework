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

#ifndef CAF_STREAM_STAGE_DRIVER_HPP
#define CAF_STREAM_STAGE_DRIVER_HPP

#include <tuple>
#include <vector>

#include "caf/fwd.hpp"

namespace caf {

/// Identifies an unbound sequence of messages.
template <class Input, class Output, class... HandshakeData>
class stream_stage_driver {
public:
  // -- member types -----------------------------------------------------------

  using input_type = Input;

  using output_type = Output;

  using stream_type = stream<output_type>;

  using annotated_stream_type = annotated_stream<output_type, HandshakeData...>;

  using handshake_tuple_type = std::tuple<stream_type, HandshakeData...>;

  // -- constructors, destructors, and assignment operators --------------------

  virtual ~stream_stage_driver() {
    // nop
  }

  // -- virtual functions ------------------------------------------------------

  /// Cleans up any state.
  virtual void finalize() {
    // nop
  }

  // -- pure virtual functions -------------------------------------------------

  /// Generates handshake data for the next actor in the pipeline.
  virtual handshake_tuple_type make_handshake() const = 0;

  /// Processes a single batch.
  virtual void process(std::vector<input_type>&& batch,
                       downstream<output_type>& out) = 0;
};

} // namespace caf

#endif // CAF_STREAM_STAGE_DRIVER_HPP
