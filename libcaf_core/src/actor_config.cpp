/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/actor_config.hpp"

#include "caf/abstract_actor.hpp"

namespace caf {

actor_config::actor_config(execution_unit* ptr)
  : host(ptr),
    flags(abstract_channel::is_abstract_actor_flag),
    groups(nullptr) {
  // nop
}

std::string to_string(const actor_config& x) {
  // Note: x.groups is an input range. Traversing it is emptying it, hence we
  // cannot look inside the range here.
  std::string result = "actor_config(";
  bool first = false;
  auto add = [&](int flag, const char* name) {
    if ((x.flags & flag) != 0) {
      if (first)
        first = false;
      else
        result += ", ";
      result += name;
    }
  };
  add(abstract_channel::is_actor_bind_decorator_flag, "bind_decorator_flag");
  add(abstract_channel::is_actor_dot_decorator_flag, "dot_decorator_flag");
  add(abstract_actor::is_detached_flag, "detached_flag");
  add(abstract_actor::is_blocking_flag, "blocking_flag");
  add(abstract_actor::is_hidden_flag, "hidden_flag");
  result += ")";
  return result;
}

} // namespace caf
