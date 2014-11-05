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
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_IO_MIDDLEMAN_HPP
#define CAF_IO_MIDDLEMAN_HPP

#include <map>
#include <vector>
#include <memory>
#include <thread>

#include "caf/fwd.hpp"
#include "caf/node_id.hpp"
#include "caf/actor_namespace.hpp"
#include "caf/detail/singletons.hpp"

#include "caf/io/hook.hpp"
#include "caf/io/broker.hpp"
#include "caf/io/network/multiplexer.hpp"

namespace caf {
namespace io {

/**
 * Manages brokers.
 */
class middleman : public detail::abstract_singleton {
 public:
  friend class detail::singletons;

  ~middleman();

  /**
   * Get middleman instance.
   */
  static middleman* instance();

  /**
   * Returns the broker associated with `name` or creates a
   * new instance of type `Impl`.
   */
  template <class Impl>
  intrusive_ptr<Impl> get_named_broker(atom_value name) {
    auto i = m_named_brokers.find(name);
    if (i != m_named_brokers.end()) {
      return static_cast<Impl*>(i->second.get());
    }
    intrusive_ptr<Impl> result{new Impl};
    result->launch(true, nullptr);
    m_named_brokers.insert(std::make_pair(name, result));
    return result;
  }

  /**
   * Adds `bptr` to the list of known brokers.
   */
  void add_broker(broker_ptr bptr);

  /**
   * Runs `fun` in the event loop of the middleman.
   * @note This member function is thread-safe.
   */
  template <class F>
  void run_later(F fun) {
    m_backend->post(fun);
  }

  /**
   * Returns the IO backend used by this middleman.
   */
  inline network::multiplexer& backend() {
    return *m_backend;
  }

  /**
   * Invokes the callback(s) associated with given event.
   */
  template <hook::event_type Event, typename... Ts>
  void notify(Ts&&... ts) {
    if (m_hooks) {
      m_hooks->handle<Event>(std::forward<Ts>(ts)...);
    }
  }

  /**
   * Adds a new hook to the middleman.
   */
  template<class C, typename... Ts>
  void add_hook(Ts&&... args) {
    // if only we could move a unique_ptr into a lambda in C++11
    auto ptr = new C(std::forward<Ts>(args)...);
    backend().dispatch([=] {
      ptr->next.swap(m_hooks);
      m_hooks.reset(ptr);
    });
  }

  /** @cond PRIVATE */

  // stops the singleton
  void stop() override;

  // deletes the singleton
  void dispose() override;

  // initializes the singleton
  void initialize() override;

  /** @endcond */

 private:
  // guarded by singleton-getter `instance`
  middleman();
  // networking backend
  std::unique_ptr<network::multiplexer> m_backend;
  // prevents backend from shutting down unless explicitly requested
  network::multiplexer::supervisor_ptr m_supervisor;
  // runs the backend
  std::thread m_thread;
  // keeps track of "singleton-like" brokers
  std::map<atom_value, broker_ptr> m_named_brokers;
  // keeps track of anonymous brokers
  std::set<broker_ptr> m_brokers;
  // user-defined hooks
  hook_uptr m_hooks;
};

} // namespace io
} // namespace caf

#endif // CAF_IO_MIDDLEMAN_HPP
