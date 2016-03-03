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

#ifndef CAF_ACTOR_SYSTEM_CONFIG_HPP
#define CAF_ACTOR_SYSTEM_CONFIG_HPP

#include <atomic>
#include <string>
#include <functional>
#include <type_traits>
#include <unordered_map>

#include "caf/actor_system.hpp"
#include "caf/actor_factory.hpp"
#include "caf/type_erased_value.hpp"

#include "caf/detail/safe_equal.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf {

class actor_system_config {
public:
  friend class actor_system;

  using module_factory = std::function<actor_system::module* (actor_system&)>;

  using module_factories = std::vector<module_factory>;

  using value_factory = std::function<type_erased_value_ptr ()>;

  using value_factories_by_name = std::unordered_map<std::string, value_factory>;

  using value_factories_by_rtti = std::unordered_map<std::type_index, value_factory>;

  using actor_factories = std::unordered_map<std::string, actor_factory>;

  using portable_names = std::unordered_map<std::type_index, std::string>;

  using error_renderer = std::function<std::string (uint8_t, const std::string&)>;

  using error_renderers = std::unordered_map<atom_value, error_renderer>;

  actor_system_config();

  actor_system_config(int argc, char** argv);

  actor_system_config& add_actor_factory(std::string name, actor_factory fun);

  template <class T, class... Ts>
  actor_system_config& add_actor_type(std::string name) {
    return add_actor_factory(std::move(name), make_actor_factory<T, Ts...>());
  }

  template <class F>
  actor_system_config& add_actor_type(std::string name, F f) {
    return add_actor_factory(std::move(name), make_actor_factory(std::move(f)));
  }

  template <class T>
  actor_system_config& add_message_type(std::string name) {
    static_assert(std::is_empty<T>::value
                  || (detail::is_comparable<T, T>::value
                      && std::is_default_constructible<T>::value
                      && std::is_copy_constructible<T>::value),
                  "T must provide default and copy constructors "
                  "as well as operator==");
    static_assert(detail::is_serializable<T>::value, "T must be serializable");
    type_names_by_rtti_.emplace(std::type_index(typeid(T)), name);
    value_factories_by_name_.emplace(std::move(name), &make_type_erased<T>);
    value_factories_by_rtti_.emplace(std::type_index(typeid(T)),
                                     &make_type_erased<T>);
    return *this;
  }

  /**
   * Enables the actor system to convert errors of this error category
   * to human-readable strings via `renderer`.
   */
  actor_system_config& add_error_category(atom_value category,
                                          error_renderer renderer);

  template <class T, class... Ts>
  actor_system_config& load() {
    module_factories_.push_back([](actor_system& sys) -> actor_system::module* {
      return T::make(sys, detail::type_list<Ts...>{});
    });
    return *this;
  }

  /// Stores CLI arguments that were not consumed by CAF.
  message args_remainder;

  /// Sets the parameter `name` to `val`.
  using config_value = variant<std::string, double, int64_t, bool, atom_value>;

  // Config parameters of scheduler.
  atom_value scheduler_policy;
  size_t scheduler_max_threads;
  size_t scheduler_max_throughput;
  bool scheduler_enable_profiling;
  size_t scheduler_profiling_ms_resolution;
  std::string scheduler_profiling_output_file;

  // Config parameters of middleman.
  atom_value middleman_network_backend;
  bool middleman_enable_automatic_connections;
  size_t middleman_max_consecutive_reads;

  // System parameters that are set while initializing modules.
  node_id network_id;
  proxy_registry* network_proxies;

  // Config parameters of RIAC probes.
  std::string nexus_host;
  uint16_t nexus_port;

private:
  value_factories_by_name value_factories_by_name_;
  value_factories_by_rtti value_factories_by_rtti_;
  portable_names type_names_by_rtti_;
  actor_factories actor_factories_;
  module_factories module_factories_;
  error_renderers error_renderers_;
};

} // namespace caf

#endif //CAF_ACTOR_SYSTEM_CONFIG_HPP
