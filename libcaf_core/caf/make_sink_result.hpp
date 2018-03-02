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

#ifndef CAF_MAKE_SINK_RESULT_HPP
#define CAF_MAKE_SINK_RESULT_HPP

#include "caf/fwd.hpp"
#include "caf/stream_slot.hpp"
#include "caf/stream_sink.hpp"

#include "caf/detail/type_traits.hpp"

#include "caf/meta/type_name.hpp"

namespace caf {

/// Wraps the result of a `make_sink` or `add_output_path` function call.
template <class In, class Result>
class make_sink_result {
public:
  // -- member types -----------------------------------------------------------

  /// A single element.
  using value_type = In;

  /// Fully typed stream manager as returned by `make_sink`.
  using type = stream_sink<In, Result>;

  /// Pointer to a fully typed stream manager.
  using ptr_type = intrusive_ptr<type>;

  /// Type of the final result message.
  using result_type = stream_result<Result>;

  // -- constructors, destructors, and assignment operators --------------------

  make_sink_result(make_sink_result&&) = default;

  make_sink_result(const make_sink_result&) = default;

  make_sink_result(stream_slot in_slot, ptr_type ptr)
      : in_(in_slot),
        ptr_(std::move(ptr)) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  /// Returns the output slot.
  inline stream_slot in() const {
    return in_;
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
                                                 make_sink_result& x) {
    return f(meta::type_name("make_sink_result"), x.in_);
  }

private:
  // -- member variables -------------------------------------------------------

  stream_slot in_;
  ptr_type ptr_;
};

} // namespace caf

#endif // CAF_MAKE_SINK_RESULT_HPP
