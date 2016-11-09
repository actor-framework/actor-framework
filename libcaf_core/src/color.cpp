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

#include "caf/color.hpp"

#include "caf/config.hpp"

namespace caf {

const char* color(color_value value, color_face face) {
#ifdef CAF_MSVC
  return "";
#else
  const char* colors[9][2] = {
    {"\033[0m", "\033[0m"},          // reset
    {"\033[30m", "\033[1m\033[30m"}, // black
    {"\033[31m", "\033[1m\033[31m"}, // red
    {"\033[32m", "\033[1m\033[32m"}, // green
    {"\033[33m", "\033[1m\033[33m"}, // yellow
    {"\033[34m", "\033[1m\033[34m"}, // blue
    {"\033[35m", "\033[1m\033[35m"}, // magenta
    {"\033[36m", "\033[1m\033[36m"}, // cyan
    {"\033[37m", "\033[1m\033[37m"}  // white
  };
  return colors[static_cast<size_t>(value)][static_cast<size_t>(face)];
#endif
}

} // namespace caf
