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

#ifndef CAF_STREAM_SOURCE_DRIVER_HPP
#define CAF_STREAM_SOURCE_DRIVER_HPP

#include <tuple>

#include "caf/fwd.hpp"

namespace caf {

/// Identifies an unbound sequence of messages.
template <class Scatterer, class... Ts>
class stream_source_driver {
public:
  // -- member types -----------------------------------------------------------

  /// Policy for distributing data to outbound paths.
  using scatterer_type = Scatterer;

  /// Element type of the output stream.
  using output_type = typename scatterer_type::value_type;

  /// Type of the output stream.
  using stream_type = stream<output_type>;

  /// Tuple for creating the `open_stream_msg` handshake.
  using handshake_tuple_type = std::tuple<stream_type, Ts...>;

  /// Implemented `stream_source` interface.
  using source_type = stream_source<output_type, scatterer_type, Ts...>;

  /// Smart pointer to the interface type.
  using source_ptr_type = intrusive_ptr<source_type>;

  /// Type of the output stream including handshake argument types.
  using output_stream_type = output_stream<output_type, std::tuple<Ts...>,
                                           source_ptr_type>;

  // -- constructors, destructors, and assignment operators --------------------

  virtual ~stream_source_driver() {
    // nop
  }

  // -- virtual functions ------------------------------------------------------

  /// Cleans up any state.
  virtual void finalize(const error&) {
    // nop
  }

  // -- pure virtual functions -------------------------------------------------

  /// Generates handshake data for the next actor in the pipeline.
  virtual handshake_tuple_type make_handshake(stream_slot slot) const = 0;

  /// Generates more stream elements.
  virtual void pull(downstream<output_type>& out, size_t num) = 0;

  /// Returns `true` if the source is done sending, otherwise `false`.
  virtual bool done() const noexcept = 0;
};

} // namespace caf

#endif // CAF_STREAM_SOURCE_DRIVER_HPP
