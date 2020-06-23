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

#include <string>

#include "caf/config_option_set.hpp"
#include "caf/config_value.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/dictionary.hpp"
#include "caf/settings.hpp"

namespace caf::detail {

class config_consumer;
class config_value_consumer;

/// Consumes a list of config values.
class CAF_CORE_EXPORT config_list_consumer {
public:
  // -- constructors, destructors, and assignment operators --------------------

  config_list_consumer(const config_option_set* options,
                       config_consumer* parent);

  config_list_consumer(const config_option_set* options,
                       config_list_consumer* parent);

  explicit config_list_consumer(config_value_consumer* parent);

  config_list_consumer(config_list_consumer&&) = default;

  config_list_consumer& operator=(config_list_consumer&&) = default;

  // -- properties -------------------------------------------------------------

  pec end_list();

  config_list_consumer begin_list() {
    return config_list_consumer{options_, this};
  }

  config_consumer begin_map();

  template <class T>
  void value(T&& x) {
    xs_.emplace_back(std::forward<T>(x));
  }

  std::string qualified_key();

private:
  // -- member variables -------------------------------------------------------

  const config_option_set* options_ = nullptr;
  variant<config_consumer*, config_list_consumer*, config_value_consumer*>
    parent_;
  config_value::list xs_;
};

/// Consumes a series of key-vale pairs from an application configuration.
class CAF_CORE_EXPORT config_consumer {
public:
  // -- constructors, destructors, and assignment operators --------------------

  config_consumer(const config_option_set* options, config_consumer* parent);

  config_consumer(const config_option_set* options,
                  config_list_consumer* parent);

  config_consumer(const config_option_set& options, settings& cfg);

  explicit config_consumer(settings& cfg);

  config_consumer(config_consumer&& other);

  config_consumer& operator=(config_consumer&& other);

  ~config_consumer();

  // -- properties -------------------------------------------------------------

  config_consumer begin_map() {
    return config_consumer{options_, this};
  }

  void end_map();

  config_list_consumer begin_list() {
    return config_list_consumer{options_, this};
  }

  void key(std::string name) {
    current_key_ = std::move(name);
  }

  template <class T>
  pec value(T&& x) {
    return value_impl(config_value{std::forward<T>(x)});
  }

  const std::string& current_key() {
    return current_key_;
  }

  std::string qualified_key();

private:
  void destroy();

  pec value_impl(config_value&& x);

  // -- member variables -------------------------------------------------------

  const config_option_set* options_ = nullptr;
  variant<none_t, config_consumer*, config_list_consumer*> parent_;
  settings* cfg_ = nullptr;
  std::string current_key_;
  std::string category_;
};

/// Consumes a single configuration value.
class CAF_CORE_EXPORT config_value_consumer {
public:
  config_value result;

  template <class T>
  void value(T&& x) {
    result = config_value{std::forward<T>(x)};
  }

  config_list_consumer begin_list();

  config_consumer begin_map();
};

} // namespace caf::detail
