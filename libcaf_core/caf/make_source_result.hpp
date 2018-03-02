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

#ifndef CAF_MAKE_SOURCE_RESULT_HPP
#define CAF_MAKE_SOURCE_RESULT_HPP

#include "caf/fwd.hpp"
#include "caf/stream_slot.hpp"
#include "caf/stream_source.hpp"

#include "caf/detail/type_traits.hpp"

#include "caf/meta/type_name.hpp"

namespace caf {

/// Wraps the result of a `make_source` or `add_output_path` function call.
template <class T, class Scatterer, class... Ts>
class make_source_result {
public:
  // -- member types -----------------------------------------------------------

  /// Type of a single element.
  using value_type = T;

  /// Fully typed stream manager as returned by `make_source`.
  using type = stream_source<T, Scatterer, Ts...>;

  /// Pointer to a fully typed stream manager.
  using ptr_type = intrusive_ptr<type>;

  // -- constructors, destructors, and assignment operators --------------------

  make_source_result(make_source_result&&) = default;

  make_source_result(const make_source_result&) = default;

  make_source_result(stream_slot out_slot, ptr_type ptr)
      : out_(out_slot),
        ptr_(std::move(ptr)) {
    // nop
  }

  // -- properties -------------------------------------------------------------

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
                                                 make_source_result& x) {
    return f(meta::type_name("make_source_result"), x.out_);
  }

private:
  // -- member variables -------------------------------------------------------

  stream_slot out_;
  ptr_type ptr_;
};

/// Helper type for defining a `make_source_result` from a `Scatterer` plus
/// additional handshake types.
template <class Scatterer, class... Ts>
using make_source_result_t =
  make_source_result<typename Scatterer::value_type, Scatterer,
                     detail::decay_t<Ts>...>;

} // namespace caf

#endif // CAF_MAKE_SOURCE_RESULT_HPP
