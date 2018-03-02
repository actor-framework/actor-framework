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

#ifndef CAF_MAKE_STAGE_RESULT_HPP
#define CAF_MAKE_STAGE_RESULT_HPP

#include "caf/fwd.hpp"
#include "caf/stream_slot.hpp"
#include "caf/stream_stage.hpp"

#include "caf/detail/type_traits.hpp"

#include "caf/meta/type_name.hpp"

namespace caf {

/// Identifies an unbound sequence of elements annotated with the additional
/// handshake arguments emitted to the next stage.
template <class In, class Result, class Out, class Scatterer, class... Ts>
class make_stage_result {
public:
  // -- member types -----------------------------------------------------------

  /// Type of a single element.
  using value_type = Out;

  /// Fully typed stream manager as returned by `make_stage`.
  using ptr_type = stream_stage_ptr<In, Result, Out, Scatterer, Ts...>;

  // -- constructors, destructors, and assignment operators --------------------

  make_stage_result(make_stage_result&&) = default;

  make_stage_result(const make_stage_result&) = default;

  make_stage_result(stream_slot in_slot, stream_slot out_slot, ptr_type ptr)
      : in_(in_slot),
        out_(out_slot),
        ptr_(std::move(ptr)) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  /// Returns the slot of the origin stream if `ptr()` is a stage or 0 if
  /// `ptr()` is a source.
  inline stream_slot in() const {
    return in_;
  }

  /// Returns the output slot.
  inline stream_slot out() const {
    return out_;
  }

  /// Returns the handler assigned to this stream on this actor.
  inline ptr_type& ptr() noexcept {
    return ptr_;
  }

  /// Returns the handler assigned to this stream on this actor.
  inline const ptr_type& ptr() const noexcept {
    return ptr_;
  }

  // -- serialization support --------------------------------------------------

  template <class Inspector>
  friend typename Inspector::result_type inspect(Inspector& f,
                                                 make_stage_result& x) {
    return f(meta::type_name("make_stage_result"), x.in_, x.out_);
  }

private:
  // -- member variables -------------------------------------------------------

  stream_slot in_;
  stream_slot out_;
  ptr_type ptr_;
};

/// Helper type for defining a `make_stage_result` from a `Scatterer` plus
/// additional handshake types. Hardwires `message` as result type.
template <class In, class Scatterer, class... Ts>
using make_stage_result_t =
  make_stage_result<In, message, typename Scatterer::value_type, Scatterer,
                    detail::decay_t<Ts>...>;

} // namespace caf

#endif // CAF_MAKE_STAGE_RESULT_HPP
