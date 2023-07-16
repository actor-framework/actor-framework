// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/actor_config.hpp"

#include "caf/abstract_actor.hpp"

namespace caf {

actor_config::actor_config(execution_unit* host, local_actor* parent)
  : host(host),
    parent(parent),
    flags(abstract_channel::is_abstract_actor_flag),
    groups(nullptr) {
  // nop
}

std::string to_string(const actor_config& x) {
  // Note: x.groups is an input range. Traversing it is emptying it, hence we
  // cannot look inside the range here.
  std::string result = "actor_config(";
  auto add = [&](int flag, const char* name) {
    if ((x.flags & flag) != 0) {
      if (result.back() != '(')
        result += ", ";
      result += name;
    }
  };
  add(abstract_channel::is_actor_bind_decorator_flag, "bind_decorator_flag");
  add(abstract_channel::is_actor_dot_decorator_flag, "dot_decorator_flag");
  add(abstract_actor::is_detached_flag, "detached_flag");
  add(abstract_actor::is_blocking_flag, "blocking_flag");
  add(abstract_actor::is_hidden_flag, "hidden_flag");
  result += ')';
  return result;
}

} // namespace caf
