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
template <class Input, class DownstreamManager>
class stream_stage_driver {
public:
  // -- member types -----------------------------------------------------------

  /// Element type of the input stream.
  using input_type = Input;

  /// Policy for distributing data to outbound paths.
  using downstream_manager_type = DownstreamManager;

  /// Element type of the output stream.
  using output_type = typename downstream_manager_type::output_type;

  /// Type of the output stream.
  using stream_type = stream<output_type>;

  /// Implemented `stream_stage` interface.
  using stage_type = stream_stage<input_type, output_type, DownstreamManager>;

  /// Smart pointer to the interface type.
  using stage_ptr_type = intrusive_ptr<stage_type>;

  // -- constructors, destructors, and assignment operators --------------------

  virtual ~stream_stage_driver() {
    // nop
  }

  // -- virtual functions ------------------------------------------------------

  /// Processes a single batch.
  virtual void process(downstream<output_type>& out,
                       std::vector<input_type>& batch) = 0;

  /// Cleans up any state.
  virtual void finalize(const error&) {
    // nop
  }
};

} // namespace caf

#endif // CAF_STREAM_STAGE_DRIVER_HPP
