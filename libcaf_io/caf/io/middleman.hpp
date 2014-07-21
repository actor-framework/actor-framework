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

#include "caf/io/broker.hpp"
#include "caf/io/network.hpp"

namespace caf {
namespace io {

/**
 * @brief Manages brokers.
 */
class middleman : public detail::abstract_singleton {

  friend class detail::singletons;

 public:

  /**
   * @brief Get middleman instance.
   */
  static middleman* instance();

  ~middleman();

  /**
   * @brief Returns the broker associated with @p name.
   */
  template <class Impl>
  intrusive_ptr<Impl> get_named_broker(atom_value name);

  /**
   * @brief Adds @p bptr to the list of known brokers.
   */
  void add_broker(broker_ptr bptr);

  /**
   * @brief Runs @p fun in the event loop of the middleman.
   * @note This member function is thread-safe.
   */
  template <class F>
  void run_later(F fun) {
    m_backend.dispatch(fun);
  }

  /**
   * @brief Returns the IO backend used by this middleman.
   */
  inline network::multiplexer& backend() { return m_backend; }

  /** @cond PRIVATE */

  // stops uninitialized instances
  void dispose() override;

  // stops an initialized singleton
  void stop() override;

  // initializes a singleton
  void initialize() override;

  /** @endcond */

 private:

  middleman();

  network::multiplexer m_backend;  // networking backend
  network::supervisor* m_supervisor; // keeps backend busy

  std::thread m_thread; // runs the backend

  std::map<atom_value, broker_ptr> m_named_brokers;

  std::set<broker_ptr> m_brokers;

};

template <class Impl>
intrusive_ptr<Impl> middleman::get_named_broker(atom_value name) {
  auto i = m_named_brokers.find(name);
  if (i != m_named_brokers.end()) return static_cast<Impl*>(i->second.get());
  intrusive_ptr<Impl> result{new Impl};
  result->launch(true, nullptr);
  m_named_brokers.insert(std::make_pair(name, result));
  return result;
}

} // namespace io
} // namespace caf

#endif // CAF_IO_MIDDLEMAN_HPP
