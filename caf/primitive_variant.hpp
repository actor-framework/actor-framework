/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/

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

using primitive_variant = variant<
                              int8_t,
                              int16_t,
                              int32_t,
                              int64_t,
                              uint8_t,
                              uint16_t,
                              uint32_t,
                              uint64_t,
                              float,
                              double,
                              long double,
                              std::string,
                              std::u16string,
                              std::u32string,
                              atom_value
                          >;

} // namespace caf

#endif // CAF_PRIMITIVE_VARIANT_HPP
