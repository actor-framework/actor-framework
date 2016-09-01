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

#ifndef CAF_META_OMITTABLE_IF_EMPTY_HPP
#define CAF_META_OMITTABLE_IF_EMPTY_HPP

#include "caf/meta/annotation.hpp"

namespace caf {
namespace meta {

struct omittable_if_empty_t : annotation {
  constexpr omittable_if_empty_t() {
    // nop
  }
};

/// Allows an inspector to omit the following data field if it is empty.
constexpr omittable_if_empty_t omittable_if_empty() {
  return {};
}

} // namespace meta
} // namespace caf

#endif // CAF_META_OMITTABLE_IF_EMPTY_HPP
