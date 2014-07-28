/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_SINGLETON_MANAGER_HPP
#define CAF_SINGLETON_MANAGER_HPP

#include <atomic>
#include <cstddef> // size_t

#include "caf/fwd.hpp"

namespace caf {
namespace detail {

class abstract_singleton {

 public:

  virtual ~abstract_singleton();

  virtual void dispose() = 0;

  virtual void stop() = 0;

  virtual void initialize() = 0;

};

class singletons {

  singletons() = delete;

 public:

  static constexpr size_t max_plugin_singletons = 3;

  static constexpr size_t middleman_plugin_id = 0;   // io lib

  static constexpr size_t opencl_plugin_id = 1;    // OpenCL lib

  static constexpr size_t probe_plugin_id = 2;     // probe hooks

  static logging* get_logger();

  static node_id get_node_id();

  static scheduler::abstract_coordinator* get_scheduling_coordinator();

  // returns false if singleton is already defined
  static bool set_scheduling_coordinator(scheduler::abstract_coordinator*);

  static group_manager* get_group_manager();

  static actor_registry* get_actor_registry();

  static uniform_type_info_map* get_uniform_type_info_map();

  static message_data* get_tuple_dummy();

  // usually guarded by implementation-specific singleton getter
  template <class Factory>
  static abstract_singleton* get_plugin_singleton(size_t id, Factory f) {
    return lazy_get(get_plugin_singleton(id), f);
  }

  static void stop_singletons();

 private:

  static std::atomic<abstract_singleton*>& get_plugin_singleton(size_t id);

  /*
   * Type `T` has to provide: `static T* create_singleton()`,
   * `void initialize()`, `void stop()`, and `dispose()`.
   */
  template <class T, typename Factory>
  static T* lazy_get(std::atomic<T*>& ptr, Factory f) {
    T* result = ptr.load();
    while (result == nullptr) {
      auto tmp = f();
      // double check if singleton is still undefined
      if (ptr.load() == nullptr) {
        tmp->initialize();
        if (ptr.compare_exchange_weak(result, tmp)) {
          result = tmp;
        } else {
          tmp->stop();
          tmp->dispose();
        }
      } else {
        tmp->dispose();
      }
    }
    return result;
  }

  template <class T>
  static T* lazy_get(std::atomic<T*>& ptr) {
    return lazy_get(ptr, [] { return T::create_singleton(); });
  }

  template <class T>
  static void stop(std::atomic<T*>& ptr) {
    auto p = ptr.load();
    if (p) p->stop();
  }

  template <class T>
  static void dispose(std::atomic<T*>& ptr) {
    for (;;) {
      auto p = ptr.load();
      if (p == nullptr) {
        return;
      } else if (ptr.compare_exchange_weak(p, nullptr)) {
        p->dispose();
        ptr = nullptr;
        return;
      }
    }
  }

};

} // namespace detail
} // namespace caf

#endif // CAF_SINGLETON_MANAGER_HPP
