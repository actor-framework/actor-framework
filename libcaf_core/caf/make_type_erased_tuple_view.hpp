/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#ifndef CAF_MAKE_TYPE_ERASED_TUPLE_HPP
#define CAF_MAKE_TYPE_ERASED_TUPLE_HPP

#include <tuple>
#include <cstddef>
#include <cstdint>
#include <typeinfo>

#include "caf/type_erased_tuple.hpp"

#include "caf/detail/type_erased_tuple_view.hpp"

namespace caf {

/// @relates type_erased_tuple
template <class... Ts>
detail::type_erased_tuple_view<Ts...> make_type_erased_tuple_view(Ts&... xs) {
  return {xs...};
}

} // namespace caf

#endif // CAF_MAKE_TYPE_ERASED_TUPLE_HPP
