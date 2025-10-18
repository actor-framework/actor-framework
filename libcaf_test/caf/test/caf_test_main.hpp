// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/test/registry.hpp"
#include "caf/test/runner.hpp"

#include "caf/exec_main.hpp"

#define CAF_TEST_MAIN(...)                                                     \
  int main(int argc, char** argv) {                                            \
    [[maybe_unused]] auto host_init_guard = caf::detail::do_init_host_system(  \
      caf::type_list<>{}, caf::type_list<__VA_ARGS__>{});                      \
    caf::exec_main_init_meta_objects<__VA_ARGS__>();                           \
    caf::core::init_global_meta_objects();                                     \
    caf::test::registry::run_init_callbacks();                                 \
    auto runner = caf::test::runner::make();                                   \
    return runner->run(argc, argv);                                            \
  }
