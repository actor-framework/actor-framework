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

#ifndef CAF_DETAIL_EMBEDDED_HPP
#define CAF_DETAIL_EMBEDDED_HPP

#include "caf/ref_counted.hpp"
#include "caf/intrusive_ptr.hpp"

namespace caf {
namespace detail {

template <class Base>
class embedded final : public Base {
 public:
  template <class... Vs>
  embedded(intrusive_ptr<ref_counted> storage, Vs&&... vs)
      : Base(std::forward<Vs>(vs)...),
        m_storage(std::move(storage)) {
    // nop
  }

  ~embedded() {
    // nop
  }

  void request_deletion() override {
    intrusive_ptr<ref_counted> guard;
    guard.swap(m_storage);
    // this code assumes that embedded is part of pair_storage<>,
    // i.e., this object lives inside a union!
    this->~embedded();
  }

 protected:
  intrusive_ptr<ref_counted> m_storage;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_EMBEDDED_HPP

