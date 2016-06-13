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

#ifndef CAF_CONFIG_OPTION_HPP
#define CAF_CONFIG_OPTION_HPP

#include <memory>
#include <string>
#include <cstdint>
#include <cstring>
#include <functional>

#include "caf/atom.hpp"
#include "caf/message.hpp"
#include "caf/variant.hpp"
#include "caf/config_value.hpp"
#include "caf/deep_to_string.hpp"
#include "caf/static_visitor.hpp"

namespace caf {

/// Helper class to generate config readers for different input types.
class config_option {
public:
  using config_reader_sink = std::function<void (size_t, config_value&)>;

  config_option(const char* category, const char* name, const char* explanation);

  virtual ~config_option();

  inline const char* name() const {
    return name_.c_str();
  }

  inline char short_name() const {
    return short_name_;
  }

  inline const char* category() const {
    return category_;
  }

  inline const char* explanation() const {
    return explanation_;
  }

  /// Returns the full name for this config option as "<category>.<long name>".
  std::string full_name() const;

  /// Returns the held value as string.
  virtual std::string to_string() const = 0;

  /// Returns a sink function for config readers.
  virtual config_reader_sink to_sink() = 0;

  /// Returns a CLI argument parser.
  virtual message::cli_arg to_cli_arg(bool use_caf_prefix = false) = 0;

  /// Returns a human-readable type name for the visited type.
  struct type_name_visitor : static_visitor<const char*> {
    const char* operator()(const std::string&) const;
    const char* operator()(double) const;
    const char* operator()(int64_t) const;
    const char* operator()(size_t) const;
    const char* operator()(uint16_t) const;
    const char* operator()(bool) const;
    const char* operator()(atom_value) const;
  };

protected:
  // 32-bit platforms
  template <class T>
  static typename std::enable_if<sizeof(T) == sizeof(uint32_t), bool>::type
  unsigned_assign_in_range(T&, int64_t& x) {
    return x <= std::numeric_limits<T>::max();
  }

  // 64-bit platforms
  template <class T>
  static typename std::enable_if<sizeof(T) == sizeof(uint64_t), bool>::type
  unsigned_assign_in_range(T&, int64_t&) {
    return true;
  }

  template <class T, class U>
  static bool assign_config_value(T& x, U& y) {
    x = std::move(y);
    return true;
  }

  static bool assign_config_value(size_t& x, int64_t& y);

  static bool assign_config_value(uint16_t& x, int64_t& y);

  void report_type_error(size_t line, config_value& x, const char* expected);

private:
  const char* category_;
  std::string name_;
  const char* explanation_;
  char short_name_;
};

template <class T>
class config_option_impl : public config_option {
public:
  config_option_impl(T& ref, const char* ctg, const char* nm, const char* xp)
      : config_option(ctg, nm, xp),
        ref_(ref) {
    // nop
  }

  std::string to_string() const override {
    return deep_to_string(ref_);
  }

  message::cli_arg to_cli_arg(bool use_caf_prefix) override {
    std::string argname;
    if (use_caf_prefix)
      argname = "caf#";
    if (strcmp(category(), "global") != 0) {
      argname += category();
      argname += ".";
    }
    argname += name();
    if (short_name() != '\0') {
      argname += ',';
      argname += short_name();
    }
    return {std::move(argname), explanation(), ref_};
  }

  config_reader_sink to_sink() override {
    return [=](size_t ln, config_value& x) {
      // the INI parser accepts all integers as int64_t
      using cfg_type =
        typename std::conditional<
          std::is_integral<T>::value && ! std::is_same<bool, T>::value,
          int64_t,
          T
        >::type;
      if (get<cfg_type>(&x) && assign_config_value(ref_, get<cfg_type>(x)))
        return;
      type_name_visitor tnv;
      report_type_error(ln, x, tnv(ref_));
    };
  }

private:
  T& ref_;
};

template <class T>
std::unique_ptr<config_option>
make_config_option(T& storage, const char* category,
                   const char* name, const char* explanation) {
  auto ptr = new config_option_impl<T>(storage, category, name, explanation);
  return std::unique_ptr<config_option>{ptr};
}

} // namespace caf

#endif // CAF_CONFIG_OPTION_HPP
