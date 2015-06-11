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
  template <class... Ts>
  embedded(intrusive_ptr<ref_counted> storage, Ts&&... xs)
      : Base(std::forward<Ts>(xs)...),
        storage_(std::move(storage)) {
    // nop
  }

  ~embedded() {
    // nop
  }

  void request_deletion(bool) noexcept override {
    intrusive_ptr<ref_counted> guard;
    guard.swap(storage_);
    // this code assumes that embedded is part of pair_storage<>,
    // i.e., this object lives inside a union!
    this->~embedded();
  }

protected:
  intrusive_ptr<ref_counted> storage_;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_EMBEDDED_HPP

