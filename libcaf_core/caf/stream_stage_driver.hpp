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
#include "caf/message.hpp"

namespace caf {

/// Encapsulates user-provided functionality for generating a stream stage.
template <class Input, class Result, class Scatterer, class... Ts>
class stream_stage_driver {
public:
  // -- member types -----------------------------------------------------------

  /// Element type of the input stream.
  using input_type = Input;

  /// Result type shipped to the client after processing all elements of the
  /// stream.
  using result_type = Result;

  /// Policy for distributing data to outbound paths.
  using scatterer_type = Scatterer;

  /// Element type of the output stream.
  using output_type = typename scatterer_type::value_type;

  /// Type of the output stream.
  using stream_type = stream<output_type>;

  /// Type of the output stream including handshake argument types.
  using output_stream_type = output_stream<output_type, Ts...>;

  /// Tuple for creating the `open_stream_msg` handshake.
  using handshake_tuple_type = std::tuple<stream_type, Ts...>;

  /// Implemented `stream_stage` interface.
  using stage_type = stream_stage<input_type, result_type,
                                  output_type, Scatterer, Ts...>;

  /// Smart pointer to the interface type.
  using stage_ptr_type = intrusive_ptr<stage_type>;

  /// Wrapper type holding a pointer to `stage_type`.
  using make_stage_result_type =
    make_stage_result<input_type, result_type, output_type, scatterer_type,
                      Ts...>;

  // -- constructors, destructors, and assignment operators --------------------

  virtual ~stream_stage_driver() {
    // nop
  }

  // -- virtual functions ------------------------------------------------------

  /// Generates handshake data for the next actor in the pipeline.
  virtual handshake_tuple_type make_handshake(stream_slot slot) const = 0;

  /// Processes a single batch.
  virtual void process(std::vector<input_type>&& batch,
                       downstream<output_type>& out) = 0;

  /// Handles the result of an outbound path.
  virtual void add_result(message&) = 0;

  /// Produces a result message after receiving the result messages of all
  /// outbound paths and closing all paths.
  virtual message make_final_result() = 0;

  /// Cleans up any state.
  virtual void finalize(const error&) = 0;
};

} // namespace caf

#endif // CAF_STREAM_STAGE_DRIVER_HPP
