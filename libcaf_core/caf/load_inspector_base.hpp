/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
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

#include "caf/inspector_access.hpp"
#include "caf/load_inspector.hpp"

namespace caf {

template <class Subtype>
class load_inspector_base : public load_inspector {
public:
  // -- member types -----------------------------------------------------------

  using super = load_inspector;

  // -- DSL entry point --------------------------------------------------------

  template <class T>
  constexpr auto object(T&) noexcept {
    return super::object_t<Subtype>{type_name_or_anonymous<T>(), dptr()};
  }

  // -- dispatching to load/load functions -------------------------------------

  template <class T>
  bool apply_value(T& x) {
    return detail::load_value(dref(), x);
  }

  template <class Get, class Set>
  bool apply_value(Get&& get, Set&&) {
    auto&& x = get();
    return detail::load_value(dref(), x);
  }

private:
  Subtype* dptr() {
    return static_cast<Subtype*>(this);
  }

  Subtype& dref() {
    return *static_cast<Subtype*>(this);
  }
};

} // namespace caf
