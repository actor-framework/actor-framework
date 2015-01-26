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

#ifndef CAF_UNIFORM_TYPEID_HPP
#define CAF_UNIFORM_TYPEID_HPP

#include <typeinfo>

namespace caf {

class uniform_type_info;

const uniform_type_info* uniform_typeid(const std::type_info& tinf,
                                        bool allow_nullptr = false);

template <class T>
const uniform_type_info* uniform_typeid(bool allow_nullptr = false) {
  return uniform_typeid(typeid(T), allow_nullptr);
}

} // namespace caf

#endif // CAF_UNIFORM_TYPEID_HPP
