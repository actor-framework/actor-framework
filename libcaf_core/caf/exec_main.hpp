// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/init_global_meta_objects.hpp"

namespace caf {

template <class>
struct exec_main_helper;

template <>
struct exec_main_helper<detail::type_list<actor_system&>> {
  using config = actor_system_config;

  template <class F>
  auto operator()(F& fun, actor_system& sys, const config&) {
    return fun(sys);
  }
};

template <class T>
struct exec_main_helper<detail::type_list<actor_system&, const T&>> {
  using config = T;

  template <class F>
  auto operator()(F& fun, actor_system& sys, const config& cfg) {
    return fun(sys, cfg);
  }
};

template <class T>
void exec_main_init_meta_objects_single() {
  if constexpr (std::is_base_of<actor_system::module, T>::value)
    T::init_global_meta_objects();
  else
    init_global_meta_objects<T>();
}

template <class... Ts>
void exec_main_init_meta_objects() {
  (exec_main_init_meta_objects_single<Ts>(), ...);
}

template <class T>
void exec_main_load_module(actor_system_config& cfg) {
  if constexpr (std::is_base_of<actor_system::module, T>::value)
    cfg.template load<T>();
}

template <class... Ts, class F = void (*)(actor_system&)>
int exec_main(F fun, int argc, char** argv) {
  using trait = typename detail::get_callable_trait<F>::type;
  using arg_types = typename trait::arg_types;
  static_assert(detail::tl_size<arg_types>::value == 1
                  || detail::tl_size<arg_types>::value == 2,
                "main function must have one or two arguments");
  static_assert(std::is_same<typename detail::tl_head<arg_types>::type,
                             actor_system&>::value,
                "main function must take actor_system& as first parameter");
  using arg2 = typename detail::tl_at<arg_types, 1>::type;
  using decayed_arg2 = typename std::decay<arg2>::type;
  static_assert(std::is_same_v<arg2, unit_t>
                  || (std::is_base_of<actor_system_config, decayed_arg2>::value
                      && std::is_same<arg2, const decayed_arg2&>::value),
                "second parameter of main function must take a subtype of "
                "actor_system_config as const reference");
  using helper = exec_main_helper<typename trait::arg_types>;
  typename helper::config cfg;
  // Load modules.
  (exec_main_load_module<Ts>(cfg), ...);
  // Pass CLI options to config.
  if (auto err = cfg.parse(argc, argv)) {
    std::cerr << "error while parsing CLI and file options: " << to_string(err)
              << std::endl;
    return EXIT_FAILURE;
  }
  // Return immediately if a help text was printed.
  if (cfg.cli_helptext_printed)
    return EXIT_SUCCESS;
  // Initialize the actor system.
  actor_system system{cfg};
  if (cfg.slave_mode) {
    if (!cfg.slave_mode_fun) {
      std::cerr << "cannot run slave mode, I/O module not loaded" << std::endl;
      return EXIT_FAILURE;
    }
    return cfg.slave_mode_fun(system, cfg);
  }
  helper f;
  using result_type = decltype(f(fun, system, cfg));
  if constexpr (std::is_convertible_v<result_type, int>) {
    return f(fun, system, cfg);
  } else {
    f(fun, system, cfg);
    return EXIT_SUCCESS;
  }
}

} // namespace caf

namespace caf::detail {

template <class... Module>
auto do_init_host_system(type_list<Module...>, type_list<>) {
  return std::make_tuple(Module::init_host_system()...);
}

template <class... Module, class T, class... Ts>
auto do_init_host_system(type_list<Module...>, type_list<T, Ts...>) {
  if constexpr (detail::has_init_host_system_v<T>) {
    return do_init_host_system(type_list<Module..., T>{}, type_list<Ts...>{});
  } else {
    return do_init_host_system(type_list<Module...>{}, type_list<Ts...>{});
  }
}

} // namespace caf::detail

#define CAF_MAIN(...)                                                          \
  int main(int argc, char** argv) {                                            \
    [[maybe_unused]] auto host_init_guard = caf::detail::do_init_host_system(  \
      caf::detail::type_list<>{}, caf::detail::type_list<__VA_ARGS__>{});      \
    caf::exec_main_init_meta_objects<__VA_ARGS__>();                           \
    caf::core::init_global_meta_objects();                                     \
    return caf::exec_main<__VA_ARGS__>(caf_main, argc, argv);                  \
  }
