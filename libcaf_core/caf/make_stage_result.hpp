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
#include "caf/output_stream.hpp"
#include "caf/stream_slot.hpp"
#include "caf/stream_stage.hpp"

#include "caf/detail/implicit_conversions.hpp"

namespace caf {

/// Helper trait for deducing an `output_stream` from the arguments to
/// `scheduled_actor::make_stage`.
template <class In, class DownstreamManager, class... Ts>
class make_stage_result {
public:
  /// Type of a single element.
  using output_type = typename DownstreamManager::output_type;

  /// Fully typed stream manager as returned by `make_stage`.
  using stage_type = stream_stage<In, DownstreamManager>;

  /// Pointer to a fully typed stream manager.
  using stage_ptr_type = intrusive_ptr<stage_type>;

  /// The return type for `scheduled_actor::make_sink`.
  using type = output_stream<output_type, std::tuple<Ts...>, stage_ptr_type>;
};

/// Helper type for defining a `make_stage_result` from a downstream manager
/// plus additional handshake types. Hardwires `message` as result type.
template <class In, class DownstreamManager, class... Ts>
using make_stage_result_t =
  typename make_stage_result<In, DownstreamManager,
                             detail::strip_and_convert_t<Ts>...>::type;

} // namespace caf

#endif // CAF_MAKE_STAGE_RESULT_HPP
