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

#ifndef CAF_ACTOR_CONFIG_HPP
#define CAF_ACTOR_CONFIG_HPP

#include <functional>

#include "caf/fwd.hpp"
#include "caf/input_range.hpp"
#include "caf/abstract_channel.hpp"

namespace caf {

class actor_config {
public:
  execution_unit* host;
  int flags;
  input_range<const group>* groups;
  std::function<behavior (local_actor*)> init_fun;

  explicit actor_config(execution_unit* ptr = nullptr)
      : host(ptr),
        flags(abstract_channel::is_abstract_actor_flag),
        groups(nullptr) {
    // nop
  }

  inline actor_config& add_flag(int x) {
    flags |= x;
    return *this;
  }
};

} // namespace caf

#endif // CAF_ACTOR_CONFIG_HPP
