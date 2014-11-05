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

#ifndef CAF_REPLIES_TO_HPP
#define CAF_REPLIES_TO_HPP

#include "caf/detail/type_list.hpp"

namespace caf {

template <class... Is>
struct replies_to {
  template <class... Os>
  struct with {
    using input_types = detail::type_list<Is...>;
    using output_types = detail::type_list<Os...>;

  };
};

template <class... Is>
using reacts_to = typename replies_to<Is...>::template with<void>;

} // namespace caf

#endif // CAF_REPLIES_TO_HPP
