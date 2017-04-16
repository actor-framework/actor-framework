/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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
#include <vector>
#include <cstdint>
#include <cstring>
#include <functional>

#include "caf/atom.hpp"
#include "caf/error.hpp"
#include "caf/message.hpp"
#include "caf/variant.hpp"
#include "caf/config_value.hpp"
#include "caf/deep_to_string.hpp"
#include "caf/static_visitor.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {

extern const char* type_name_visitor_tbl[];

/// Helper class to generate config readers for different input types.
class config_option {
public:
  using config_reader_sink = std::function<void (size_t, config_value&,
                                                 optional<std::ostream&>)>;

  using legal_types = detail::type_list<bool, float, double, std::string,
                                        atom_value, int8_t, uint8_t, int16_t,
                                        uint16_t, int32_t, uint32_t, int64_t,
                                        uint64_t>;

  config_option(const char* cat, const char* nm, const char* expl);

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
  class type_name_visitor : public static_visitor<const char*> {
  public:
    template <class T>
    const char* operator()(const T&) const {
      static constexpr bool is_int = std::is_integral<T>::value
                                     && !std::is_same<bool, T>::value;
      static constexpr std::integral_constant<bool, is_int> tk{};
      static constexpr int index = idx<T>(tk);
      static_assert(index >= 0, "illegal type in name visitor");
      return type_name_visitor_tbl[static_cast<size_t>(index)];
    }

  private:
    // Catches non-integer types.
    template <class T>
    static constexpr int idx(std::false_type) {
      return detail::tl_index_of<legal_types, T>::value;
    }

    // Catches integer types.
    template <class T>
    static constexpr int idx(std::true_type) {
      using squashed = detail::squashed_int_t<T>;
      return detail::tl_index_of<legal_types, squashed>::value;
    }
  };

protected:
  template <class T, class U>
  static bool assign_config_value(T& x, U& y) {
    x = std::move(y);
    return true;
  }

  // Catches any integer type that is smaller than int64_t.
  template <class T>
  static typename std::enable_if<sizeof(T) < sizeof(int64_t), bool>::type
  assign_config_value(T& x, int64_t& y) {
    if (y < static_cast<int64_t>(std::numeric_limits<T>::lowest())
        || y > static_cast<int64_t>(std::numeric_limits<T>::max()))
      return false;
    x = static_cast<T>(y);
    return true;
  }

  // Catches size_t and uint64_t (yes, they differ on some compilers).
  template <class T>
  static typename std::enable_if<std::is_unsigned<T>::value
                                 && sizeof(T) == sizeof(int64_t), bool>::type
  assign_config_value(T& x, int64_t& y) {
    if (y < 0)
      return false;
    x = static_cast<uint64_t>(y);
    return true;
  }

  static bool assign_config_value(float& x, double& y) {
    if (y < static_cast<double>(std::numeric_limits<float>::lowest())
        || y > static_cast<double>(std::numeric_limits<float>::max()))
      return false;
    x = static_cast<float>(y);
    return true;
  }

  void report_type_error(size_t ln, config_value& x, const char* expected,
                         optional<std::ostream&> out);

private:
  const char* category_;
  std::string name_;
  const char* explanation_;
  char short_name_;
};

template <class T,
          bool IsInsertable = detail::can_insert_elements<T>()
                              && !std::is_same<T, std::string>::value>
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
    return [=](size_t ln, config_value& x, optional<std::ostream&> errors) {
      // the INI parser accepts all integers as int64_t
      // and all floating point numbers as doubles
      using cfg_type =
        typename std::conditional<
          std::is_integral<T>::value && !std::is_same<bool, T>::value,
          int64_t,
          typename std::conditional<
            std::is_floating_point<T>::value,
            double,
            T
            >::type
        >::type;
      if (get<cfg_type>(&x) && assign_config_value(ref_, get<cfg_type>(x)))
        return;
      type_name_visitor tnv;
      report_type_error(ln, x, tnv(ref_), errors);
    };
  }

private:
  T& ref_;
};

template <class T>
class config_option_impl<T, true> : public config_option {
public:
  using value_type = typename T::value_type;

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
    return [=](size_t ln, config_value& x, optional<std::ostream&> errors) {
      // the INI parser accepts all integers as int64_t
      // and all floating point numbers as doubles
      using cfg_type =
        typename std::conditional<
          std::is_integral<value_type>::value && 
                           !std::is_same<bool, value_type>::value,
          int64_t,
          typename std::conditional<
            std::is_floating_point<value_type>::value,
            double,
            value_type 
            >::type
        >::type;
      value_type tmp;
      if (get<cfg_type>(&x) && assign_config_value(tmp, get<cfg_type>(x))) {
        ref_.insert(ref_.end(), std::move(tmp));
        return;
      }
      type_name_visitor tnv;
      report_type_error(ln, x, tnv(tmp), errors);
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
