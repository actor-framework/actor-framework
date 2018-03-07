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
 * http://openresult_handler.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_STREAM_RESULT_HANDLER_TRAIT_HPP
#define CAF_STREAM_RESULT_HANDLER_TRAIT_HPP

#include "caf/fwd.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {

/// Deduces the input type for a stream result handler from its signature.
template <class F>
struct stream_result_handler_trait {
  static constexpr bool valid = false;
  using result = void;
};

template <class T>
struct stream_result_handler_trait<void (expected<T>)> {
  static constexpr bool valid = true;
  using result = T;
};

/// Convenience alias for extracting the function signature from `Pull` and
/// passing it to `stream_result_handler_trait`.
template <class Pull>
using stream_result_handler_trait_t =
  stream_result_handler_trait<typename detail::get_callable_trait<Pull>::fun_sig>;

} // namespace caf

#endif // CAF_STREAM_RESULT_HANDLER_TRAIT_HPP
