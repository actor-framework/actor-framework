/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#ifndef CAF_PRIMITIVE_VARIANT_HPP
#define CAF_PRIMITIVE_VARIANT_HPP

#include <new>
#include <cstdint>
#include <typeinfo>
#include <stdexcept>
#include <type_traits>

#include "caf/none.hpp"
#include "caf/variant.hpp"

#include "caf/atom.hpp"

namespace caf {

using primitive_variant = variant<int8_t, int16_t, int32_t, int64_t,
                                  uint8_t, uint16_t, uint32_t, uint64_t,
                                  float, double, long double,
                                  std::string, std::u16string, std::u32string,
                                  atom_value, bool>;

} // namespace caf

#endif // CAF_PRIMITIVE_VARIANT_HPP
