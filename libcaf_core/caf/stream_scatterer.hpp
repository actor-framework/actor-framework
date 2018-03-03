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

#ifndef CAF_STREAM_SCATTERER_HPP
#define CAF_STREAM_SCATTERER_HPP

#include <cstddef>
#include <memory>

#include "caf/actor_control_block.hpp"
#include "caf/duration.hpp"
#include "caf/fwd.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/optional.hpp"
#include "caf/stream_slot.hpp"

#include "caf/detail/unordered_flat_map.hpp"

namespace caf {

/// Type-erased policy for dispatching data to sinks.
class stream_scatterer {
public:
  // -- member types -----------------------------------------------------------

  /// Outbound path.
  using path_type = outbound_path;

  /// Pointer to an outbound path.
  using path_ptr = path_type*;

  /// Unique pointer to an outbound path.
  using path_unique_ptr = std::unique_ptr<path_type>;

  /// Maps slots to paths.
  using map_type = detail::unordered_flat_map<stream_slot, path_unique_ptr>;

  // -- constructors, destructors, and assignment operators --------------------

  explicit stream_scatterer(local_actor* self);

  virtual ~stream_scatterer();

  // -- path management --------------------------------------------------------

  /// Returns the container that stores all paths.
  inline const map_type& paths() const noexcept {
    return paths_;
  }

  /// Adds a path to `target` to the scatterer.
  /// @returns The added path on success, `nullptr` otherwise.
  virtual path_ptr add_path(stream_slots slots, strong_actor_ptr target);

  /// Removes a path from the scatterer and returns it.
  path_unique_ptr take_path(stream_slot slots) noexcept;

  /// Returns the path associated to `slots` or `nullptr`.
  path_ptr path(stream_slot slots) noexcept;

  /// Returns the stored state for `x` if `x` is a known path and associated to
  /// `sid`, otherwise `nullptr`.
  path_ptr path_at(size_t index) noexcept;

  /// Removes a path from the scatterer and returns it.
  path_unique_ptr take_path_at(size_t index) noexcept;

  /// Returns `true` if there is no data pending and no unacknowledged batch on
  /// any path.
  bool clean() const noexcept;

  /// Removes all paths gracefully.
  void close();

  /// Removes all paths with an error message.
  void abort(error reason);

  /// Returns `true` if no downstream exists and `!continuous()`,
  /// `false` otherwise.
  inline bool empty() const noexcept {
    return paths_.empty();
  }

  /// Returns the minimum amount of credit on all output paths.
  size_t min_credit() const noexcept;

  /// Returns the maximum amount of credit on all output paths.
  size_t max_credit() const noexcept;

  /// Returns the total amount of credit on all output paths, i.e., the sum of
  /// all individual credits.
  size_t total_credit() const noexcept;

  // -- state access -----------------------------------------------------------

  local_actor* self() const {
    return self_;
  }

  // -- meta information -------------------------------------------------------

  /// Returns `true` if thie scatterer belongs to a sink, i.e., terminates the
  /// stream and never has outbound paths.
  virtual bool terminal() const noexcept;

  // -- pure virtual memeber functions -----------------------------------------

  /// Sends batches to sinks.
  virtual void emit_batches() = 0;

  /// Sends batches to sinks regardless of whether or not the batches reach the
  /// desired batch size.
  virtual void force_emit_batches() = 0;

  /// Returns the currently available capacity for the output buffer.
  virtual size_t capacity() const noexcept = 0;

  /// Returns the size of the output buffer.
  virtual size_t buffered() const noexcept = 0;

  /// Returns `make_message(stream<T>{slot})`, where `T` is the value type of
  /// this scatterer.
  virtual message make_handshake_token(stream_slot slot) const = 0;

  // -- convenience functions --------------------------------------------------

  /// Removes a path from the scatterer.
  bool remove_path(stream_slots slot, const strong_actor_ptr& x,
                   error reason, bool silent);

  /// Convenience function for calling `find(x, actor_cast<actor_addr>(x))`.
  path_ptr find(stream_slot slot, const strong_actor_ptr& x);

protected:
  // -- customization points ---------------------------------------------------

  /// Emits a regular (`reason == nullptr`) or irregular (`reason != nullptr`)
  /// shutdown if `silent == false`.
  /// @warning moves `*reason` if `reason == nullptr`
  virtual void about_to_erase(map_type::iterator i, bool silent, error* reason);

  // -- member variables -------------------------------------------------------

  map_type paths_;
  local_actor* self_;
};

} // namespace caf

#endif // CAF_STREAM_SCATTERER_HPP
