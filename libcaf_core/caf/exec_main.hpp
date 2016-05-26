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

#ifndef CAF_EXEC_MAIN_HPP
#define CAF_EXEC_MAIN_HPP

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"

#include "caf/detail/type_list.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf {

template <class>
struct exec_main_helper;

template <>
struct exec_main_helper<detail::type_list<actor_system&>> {
  using config = actor_system_config;

  template <class F>
  void operator()(F& fun, actor_system& sys, config&) {
    fun(sys);
  }
};

template <class T>
struct exec_main_helper<detail::type_list<actor_system&, T&>> {
  using config = T;

  template <class F>
  void operator()(F& fun, actor_system& sys, config& cfg) {
    fun(sys, cfg);
  }
};

template <class... Ts, class F = void (*)(actor_system&)>
int exec_main(F fun, int argc, char** argv,
              const char* config_file_name = "caf-application.ini") {
  using trait = typename detail::get_callable_trait<F>::type;
  using helper = exec_main_helper<typename trait::arg_types>;
  // pass CLI options to config
  typename helper::config cfg;
  cfg.parse(argc, argv, config_file_name);
  // return immediately if a help text was printed
  if (cfg.cli_helptext_printed)
    return 0;
  // load modules
  std::initializer_list<unit_t> unused{unit_t{cfg.template load<Ts>()}...};
  CAF_IGNORE_UNUSED(unused);
  // pass config to the actor system
  actor_system system{cfg};
  if (cfg.slave_mode) {

  }
  helper f;
  f(fun, system, cfg);
  return 0;
}

} // namespace caf

#define CAF_MAIN(...)                                                          \
  int main(int argc, char** argv) {                                            \
    ::caf::exec_main<__VA_ARGS__>(caf_main, argc, argv);                       \
  }

#endif // CAF_EXEC_MAIN_HPP
