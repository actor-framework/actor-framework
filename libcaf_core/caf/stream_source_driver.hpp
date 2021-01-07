// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <tuple>

#include "caf/fwd.hpp"

namespace caf {

/// Identifies an unbound sequence of messages.
template <class DownstreamManager>
class stream_source_driver {
public:
  // -- member types -----------------------------------------------------------

  /// Type of the downstream manager, i.e., the type returned by
  /// `stream_manager::out()`.
  using downstream_manager_type = DownstreamManager;

  /// Element type of the output stream.
  using output_type = typename downstream_manager_type::output_type;

  /// Type of the output stream.
  using stream_type = stream<output_type>;

  /// Implemented `stream_source` interface.
  using source_type = stream_source<downstream_manager_type>;

  /// Smart pointer to the interface type.
  using source_ptr_type = intrusive_ptr<source_type>;

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

  /// Generates more elements for `dst`.
  virtual void pull(downstream<output_type>& dst, size_t num) = 0;

  /// Returns `true` if the source is done sending, otherwise `false`.
  virtual bool done() const noexcept = 0;
};

} // namespace caf
