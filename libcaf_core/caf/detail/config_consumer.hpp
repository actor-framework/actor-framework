// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config_option_set.hpp"
#include "caf/config_value.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/dictionary.hpp"
#include "caf/settings.hpp"

#include <string>

namespace caf::detail {

class config_consumer;
class config_value_consumer;

/// Consumes a list of config values.
class CAF_CORE_EXPORT config_list_consumer {
public:
  // -- constructors, destructors, and assignment operators --------------------

  config_list_consumer() = default;

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
    result.emplace_back(std::forward<T>(x));
  }

  config_value::list result;

  std::string qualified_key();

private:
  // -- member variables -------------------------------------------------------

  const config_option_set* options_ = nullptr;
  std::variant<none_t, config_consumer*, config_list_consumer*,
               config_value_consumer*>
    parent_;
};

/// Consumes a series of key-value pairs from an application configuration.
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
    using val_t = std::decay_t<T>;
    if constexpr (std::is_same_v<val_t, uint64_t>) {
      if (x <= INT64_MAX)
        return value_impl(config_value{static_cast<int64_t>(x)});
      else
        return pec::integer_overflow;
    } else {
      return value_impl(config_value{std::forward<T>(x)});
    }
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
  std::variant<none_t, config_consumer*, config_list_consumer*> parent_;
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
