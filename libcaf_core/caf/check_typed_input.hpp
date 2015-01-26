/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#ifndef CAF_CHECK_TYPED_INPUT_HPP
#define CAF_CHECK_TYPED_INPUT_HPP

#include "caf/detail/type_list.hpp"
#include "caf/detail/typed_actor_util.hpp"

namespace caf {

/**
 * Checks whether `R` does support an input of type `{Ls...}` via a
 * static assertion (always returns 0).
 */
template <template <class...> class R, class... Rs,
          template <class...> class L, class... Ls>
constexpr int check_typed_input(const R<Rs...>&, const L<Ls...>&) {
  static_assert(detail::tl_find_if<
                  detail::type_list<Rs...>,
                  detail::input_is<detail::type_list<Ls...>>::template eval
                >::value >= 0, "typed actor does not support given input");
  return 0;
}

} // namespace caf

#endif // CAF_CHECK_TYPED_INPUT_HPP
