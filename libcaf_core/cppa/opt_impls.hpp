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

#ifndef CPPA_DETAIL_OPT_IMPLS_HPP
#define CPPA_DETAIL_OPT_IMPLS_HPP

#include <sstream>

#include "caf/on.hpp"
#include "caf/optional.hpp"

// this header contains implementation details for opt.hpp

namespace caf {
namespace detail {

template <class T>
struct conv_arg_impl {
  using result_type = optional<T>;
  static inline result_type _(const std::string& arg) {
    std::istringstream iss(arg);
    T result;
    if (iss >> result && iss.eof()) {
      return result;
    }
    return none;
  }
};

template <>
struct conv_arg_impl<std::string> {
  using result_type = optional<std::string>;
  static inline result_type _(const std::string& arg) { return arg; }
};

template <class T>
struct conv_arg_impl<optional<T>> {
  using result_type = optional<T>;
  static inline result_type _(const std::string& arg) {
    return conv_arg_impl<T>::_(arg);
  }
};

template <bool>
class opt1_rvalue_builder;

template <class T>
struct rd_arg_storage : ref_counted {
  T& storage;
  bool set;
  std::string arg_name;
  rd_arg_storage(T& r) : storage(r), set(false) {
    // nop
  }
};

template <class T>
class rd_arg_functor {
 public:
  template <bool>
  friend class opt1_rvalue_builder;

  using storage_type = rd_arg_storage<T>;

  rd_arg_functor(const rd_arg_functor&) = default;

  rd_arg_functor(T& storage) : m_storage(new storage_type(storage)) {
    // nop
  }

  bool operator()(const std::string& arg) const {
    if (m_storage->set) {
      std::cerr << "*** error: " << m_storage->arg_name
                << " already defined" << std::endl;
      return false;
    }
    auto opt = conv_arg_impl<T>::_(arg);
    if (!opt) {
      std::cerr << "*** error: cannot convert \"" << arg << "\" to "
                << detail::demangle(typeid(T).name())
                << " [option: \"" << m_storage->arg_name << "\"]"
                << std::endl;
      return false;
    }
    m_storage->storage = *opt;
    m_storage->set = true;
    return true;
  }

 private:
  intrusive_ptr<storage_type> m_storage;
};

template <class T>
class add_arg_functor {
 public:
  template <bool>
  friend class opt1_rvalue_builder;

  using value_type = std::vector<T>;
  using storage_type = rd_arg_storage<value_type>;

  add_arg_functor(const add_arg_functor&) = default;

  add_arg_functor(value_type& storage) : m_storage(new storage_type(storage)) {
    // nop
  }

  bool operator()(const std::string& arg) const {
    auto opt = conv_arg_impl<T>::_(arg);
    if (!opt) {
      std::cerr << "*** error: cannot convert \"" << arg << "\" to "
                << detail::demangle(typeid(T))
                << " [option: \"" << m_storage->arg_name << "\"]"
                << std::endl;
      return false;
    }
    m_storage->storage.push_back(*opt);
    return true;
  }

 private:
  intrusive_ptr<storage_type> m_storage;
};

template <class T>
struct is_rd_arg : std::false_type { };

template <class T>
struct is_rd_arg<rd_arg_functor<T>> : std::true_type { };

template <class T>
struct is_rd_arg<add_arg_functor<T>> : std::true_type { };

using opt0_rvalue_builder = decltype(on<std::string>());

template <bool HasShortOpt = true>
class opt1_rvalue_builder {
 public:
  using left_type = decltype(on<std::string, std::string>());

  using right_type =
    decltype(on(std::function<optional<std::string> (const std::string&)>()));

  template <class Left, typename Right>
  opt1_rvalue_builder(char sopt, std::string lopt, Left&& lhs, Right&& rhs)
      : m_short(sopt),
        m_long(std::move(lopt)),
        m_left(std::forward<Left>(lhs)),
        m_right(std::forward<Right>(rhs)) {
    // nop
  }

  template <class Expr>
  auto operator>>(Expr expr)
  -> decltype((*(static_cast<left_type*>(nullptr)) >> expr).or_else(
               *(static_cast<right_type*>(nullptr)) >> expr)) const {
    inject_arg_name(expr);
    return (m_left >> expr).or_else(m_right >> expr);
  }

 private:
  template <class T>
  void inject_arg_name(rd_arg_functor<T>& expr) {
    expr.m_storage->arg_name = m_long;
  }

  template <class T>
  void inject_arg_name(const T&) {
    // using opts without rd_arg() or similar functor
  }

  char m_short;
  std::string m_long;
  left_type m_left;
  right_type m_right;
};

template <>
class opt1_rvalue_builder<false> {
 public:
  using sub_type =
    decltype(on(std::function<optional<std::string> (const std::string&)>()));

  template <class SubType>
  opt1_rvalue_builder(std::string lopt, SubType&& sub)
      : m_long(std::move(lopt)),
        m_sub(std::forward<SubType>(sub)) {
    // nop
  }

  template <class Expr>
  auto operator>>(Expr expr)
  -> decltype(*static_cast<sub_type*>(nullptr) >> expr) const {
    inject_arg_name(expr);
    return m_sub >> expr;
  }

 private:
  template <class T>
  inline void inject_arg_name(rd_arg_functor<T>& expr) {
    expr.m_storage->arg_name = m_long;
  }

  template <class T>
  inline void inject_arg_name(const T&) {
    // using opts without rd_arg() or similar functor
  }

  std::string m_long;
  sub_type m_sub;
};

} // namespace detail
} // namespace caf

#endif // CPPA_DETAIL_OPT_IMPLS_HPP
