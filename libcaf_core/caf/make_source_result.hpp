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

/// Helper trait for deducing an `output_stream` from the arguments to
/// `scheduled_actor::make_source`.
template <class Scatterer, class... Ts>
struct make_source_result {
  /// Type of a single element.
  using value_type = typename Scatterer::value_type;

  /// Fully typed stream manager as returned by `make_source`.
  using source_type = stream_source<value_type, Scatterer, Ts...>;

  /// Pointer to a fully typed stream manager.
  using source_ptr_type = intrusive_ptr<source_type>;

  /// The return type for `scheduled_actor::make_source`.
  using type = output_stream<value_type, std::tuple<Ts...>, source_ptr_type>;
};

/// Helper type for defining an `output_stream` from a `Scatterer` plus
/// the types of the handshake arguments.
template <class Scatterer, class... Ts>
using make_source_result_t =
  typename make_source_result<Scatterer, detail::decay_t<Ts>...>::type;

} // namespace caf

#endif // CAF_MAKE_SOURCE_RESULT_HPP
