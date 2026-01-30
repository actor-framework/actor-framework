// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/actor_config.hpp"

#include "caf/test/test.hpp"

#include "caf/abstract_actor.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/scoped_actor.hpp"

using namespace caf;

TEST("default constructor initializes members to nullptr") {
  actor_config cfg;
  check(cfg.sched == nullptr);
  check(cfg.parent == nullptr);
  check(cfg.init_fun.is_nullptr());
  check(cfg.mbox_factory == nullptr);
  check_eq(cfg.flags, 0);
}

TEST("constructor with scheduler sets sched member") {
  actor_system_config sys_cfg;
  actor_system sys{sys_cfg};
  actor_config cfg{&sys.scheduler()};
  check(cfg.sched == &sys.scheduler());
  check(cfg.parent == nullptr);
  check_eq(cfg.flags, 0);
}

TEST("constructor with scheduler and parent sets both members") {
  actor_system_config sys_cfg;
  actor_system sys{sys_cfg};
  scoped_actor self{sys};
  actor_config cfg{&sys.scheduler(), self.ptr()};
  check(cfg.sched == &sys.scheduler());
  check(cfg.parent == self.ptr());
  check_eq(cfg.flags, 0);
}

TEST("to_string with no flags returns empty parentheses") {
  actor_config cfg;
  check_eq(to_string(cfg), "actor_config()");
}

TEST("to_string with detached_flag") {
  actor_config cfg;
  cfg.add_flag(abstract_actor::is_detached_flag);
  check_eq(to_string(cfg), "actor_config(detached_flag)");
}

TEST("to_string with blocking_flag") {
  actor_config cfg;
  cfg.add_flag(abstract_actor::is_blocking_flag);
  check_eq(to_string(cfg), "actor_config(blocking_flag)");
}

TEST("to_string with hidden_flag") {
  actor_config cfg;
  cfg.add_flag(abstract_actor::is_hidden_flag);
  check_eq(to_string(cfg), "actor_config(hidden_flag)");
}

TEST("to_string with detached and blocking flags") {
  actor_config cfg;
  cfg.add_flag(abstract_actor::is_detached_flag)
    .add_flag(abstract_actor::is_blocking_flag);
  auto result = to_string(cfg);
  check_eq(result.front(), 'a');
  check_eq(result.back(), ')');
  check(result.find("detached_flag") != std::string::npos);
  check(result.find("blocking_flag") != std::string::npos);
  check(result.find(", ") != std::string::npos);
}

TEST("to_string with all three flags") {
  actor_config cfg;
  cfg.add_flag(abstract_actor::is_detached_flag)
    .add_flag(abstract_actor::is_blocking_flag)
    .add_flag(abstract_actor::is_hidden_flag);
  auto result = to_string(cfg);
  check_eq(result.front(), 'a');
  check_eq(result.back(), ')');
  check(result.find("detached_flag") != std::string::npos);
  check(result.find("blocking_flag") != std::string::npos);
  check(result.find("hidden_flag") != std::string::npos);
}

TEST("add_flag returns reference for chaining") {
  actor_config cfg;
  auto& ref = cfg.add_flag(abstract_actor::is_detached_flag);
  check_eq(&ref, &cfg);
  check_eq(cfg.flags, abstract_actor::is_detached_flag);
}
