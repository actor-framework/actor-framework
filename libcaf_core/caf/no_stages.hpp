/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#ifndef CAF_NO_STAGES_HPP
#define CAF_NO_STAGES_HPP

#include "caf/mailbox_element.hpp"

namespace caf {

/// Convenience tag type for producing empty forwarding stacks.
struct no_stages_t {
  constexpr no_stages_t() {
    // nop
  }

  inline operator mailbox_element::forwarding_stack() const {
    return {};
  }
};

/// Convenience tag for producing empty forwarding stacks.
constexpr no_stages_t no_stages = no_stages_t{};

} // namespace caf

#endif // CAF_NO_STAGES_HPP
