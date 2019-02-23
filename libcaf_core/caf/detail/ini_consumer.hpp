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
#include "caf/dictionary.hpp"
#include "caf/settings.hpp"

namespace caf {
namespace detail {

class ini_consumer;
class ini_list_consumer;
class ini_map_consumer;

class abstract_ini_consumer {
public:
  // -- constructors, destructors, and assignment operators --------------------

  explicit abstract_ini_consumer(abstract_ini_consumer* parent = nullptr);

  abstract_ini_consumer(const abstract_ini_consumer&) = delete;

  abstract_ini_consumer& operator=(const abstract_ini_consumer&) = delete;

  virtual ~abstract_ini_consumer();

  // -- properties -------------------------------------------------------------

  virtual void value_impl(config_value&& x) = 0;

  template <class T>
  void value(T&& x) {
    value_impl(config_value{std::forward<T>(x)});
  }

  inline abstract_ini_consumer* parent() {
    return parent_;
  }

  ini_map_consumer begin_map();

  ini_list_consumer begin_list();

protected:
  // -- member variables -------------------------------------------------------

  abstract_ini_consumer* parent_;
};

class ini_map_consumer : public abstract_ini_consumer {
public:
  // -- member types -----------------------------------------------------------

  using super = abstract_ini_consumer;

  using map_type = config_value::dictionary;

  using iterator = map_type::iterator;

  // -- constructors, destructors, and assignment operators --------------------

  ini_map_consumer(abstract_ini_consumer* ptr);

  ini_map_consumer(ini_map_consumer&& other);

  ~ini_map_consumer() override;

  // -- properties -------------------------------------------------------------

  void end_map();

  void key(std::string name);

  void value_impl(config_value&& x) override;

private:
  // -- member variables -------------------------------------------------------

  map_type xs_;
  iterator i_;
};

class ini_list_consumer : public abstract_ini_consumer {
public:
  // -- member types -----------------------------------------------------------

  using super = abstract_ini_consumer;

  // -- constructors, destructors, and assignment operators --------------------

  ini_list_consumer(abstract_ini_consumer* ptr);

  ini_list_consumer(ini_list_consumer&& other);

  // -- properties -------------------------------------------------------------

  void end_list();

  void value_impl(config_value&& x) override;

private:
  // -- member variables -------------------------------------------------------

  config_value::list xs_;
};

/// Consumes a single value from an INI parser.
class ini_value_consumer : public abstract_ini_consumer {
public:
  // -- member types -----------------------------------------------------------

  using super = abstract_ini_consumer;

  // -- constructors, destructors, and assignment operators --------------------

  explicit ini_value_consumer(abstract_ini_consumer* parent = nullptr);

  // -- properties -------------------------------------------------------------

  void value_impl(config_value&& x) override;

  // -- member variables -------------------------------------------------------

  config_value result;
};

/// Consumes a config category.
class ini_category_consumer : public abstract_ini_consumer {
public:
  // -- member types -----------------------------------------------------------

  using super = abstract_ini_consumer;

  // -- constructors, destructors, and assignment operators --------------------

  ini_category_consumer(ini_consumer* parent, std::string category);

  ini_category_consumer(ini_category_consumer&&);

  // -- properties -------------------------------------------------------------

  void end_map();

  void key(std::string name);

  void value_impl(config_value&& x) override;

private:
  // -- properties -------------------------------------------------------------

  ini_consumer* dparent();

  // -- member variables -------------------------------------------------------

  std::string category_;
  config_value::dictionary xs_;
  std::string current_key;
};

/// Consumes a series of dictionaries forming a application configuration.
class ini_consumer : public abstract_ini_consumer {
public:
  // -- friends ----------------------------------------------------------------

  friend class ini_category_consumer;

  // -- member types -----------------------------------------------------------

  using super = abstract_ini_consumer;

  using config_map = dictionary<config_value::dictionary>;

  // -- constructors, destructors, and assignment operators --------------------

  ini_consumer(config_option_set& options, settings& cfg);

  ini_consumer(ini_consumer&&) = default;

  // -- properties -------------------------------------------------------------

  ini_category_consumer begin_map();

  void key(std::string name);

  void value_impl(config_value&& x) override;

private:
  // -- member variables -------------------------------------------------------

  config_option_set& options_;
  settings& cfg_;
  std::string current_key_;
  std::vector<error> warnings_;
};

} // namespace detail
} // namespace caf
