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

#ifndef CAF_TO_STRING_HPP
#define CAF_TO_STRING_HPP

#include <string>

#include "caf/error.hpp"
#include "caf/deep_to_string.hpp"

#include "caf/detail/type_traits.hpp"
#include "caf/detail/stringification_inspector.hpp"

namespace caf {

template <class T,
          class E = typename std::enable_if<
                      detail::is_inspectable<
                        detail::stringification_inspector,
                        T
                      >::value
                    >::type>
std::string to_string(const T& x) {
  std::string res;
  detail::stringification_inspector f{res};
  inspect(f, const_cast<T&>(x));
  return res;
}

} // namespace caf

#endif // CAF_TO_STRING_HPP
