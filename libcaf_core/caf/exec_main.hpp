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

#pragma once

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
struct exec_main_helper<detail::type_list<actor_system&, const T&>> {
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
  using arg_types = typename trait::arg_types;
  static_assert(detail::tl_size<arg_types>::value == 1
                || detail::tl_size<arg_types>::value == 2,
                "main function must have one or two arguments");
  static_assert(std::is_same<
                  typename detail::tl_head<arg_types>::type,
                  actor_system&
                >::value,
                "main function must take actor_system& as first parameter");
  using arg2 = typename detail::tl_at<arg_types, 1>::type;
  using decayed_arg2 = typename std::decay<arg2>::type;
  static_assert(std::is_same<arg2, unit_t>::value
                || (std::is_base_of<actor_system_config, decayed_arg2>::value
                    && std::is_same<arg2, const decayed_arg2&>::value),
                "second parameter of main function must take a subtype of "
                "actor_system_config as const reference");
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
    if (!cfg.slave_mode_fun) {
      std::cerr << "cannot run slave mode, I/O module not loaded" << std::endl;
      return 1;
    }
    return cfg.slave_mode_fun(system, cfg);
  }
  helper f;
  f(fun, system, cfg);
  return 0;
}

} // namespace caf

#define CAF_MAIN(...)                                                          \
  int main(int argc, char** argv) {                                            \
    return ::caf::exec_main<__VA_ARGS__>(caf_main, argc, argv);                \
  }

