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

#ifndef CAF_SINGLETON_MIXIN_HPP
#define CAF_SINGLETON_MIXIN_HPP

#include <utility>

#include "caf/detail/logging.hpp"

namespace caf {
namespace detail {

class singletons;

// a mixin for simple singleton classes
template <class Derived, class Base = void>
class singleton_mixin : public Base {
private:
  friend class singletons;

  static Derived* create_singleton() {
    return new Derived;
  }

  void dispose() {
    delete this;
  }

  void stop() {
    // nop
  }

  void initialize() {
    CAF_LOG_TRACE("");
  }

protected:
  template <class... Ts>
  singleton_mixin(Ts&&... xs) : Base(std::forward<Ts>(xs)...) {
    // nop
  }

  virtual ~singleton_mixin() {
    CAF_LOG_TRACE("");
  }
};

template <class Derived>
class singleton_mixin<Derived, void> {
private:
  friend class singletons;

  static Derived* create_singleton() {
    return new Derived;
  }

  void dispose() {
    delete this;
  }

  void stop() {
    // nop
  }

  void initialize() {
    CAF_LOG_TRACE("");
  }

protected:
  virtual ~singleton_mixin() {
    CAF_LOG_TRACE("");
  }
};

} // namespace detail
} // namespace caf

#endif // CAF_SINGLETON_MIXIN_HPP
