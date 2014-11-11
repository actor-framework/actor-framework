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
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_FUNCTOR_ATTACHABLE_HPP
#define CAF_FUNCTOR_ATTACHABLE_HPP

#include "caf/attachable.hpp"

#include "caf/detail/type_list.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf {
namespace detail {

template <class F,
          int Args = tl_size<typename get_callable_trait<F>::arg_types>::value>
struct functor_attachable : attachable {
  static_assert(Args == 2, "Only 1 and 2 arguments for F are supported");
  F m_functor;
  functor_attachable(F arg) : m_functor(std::move(arg)) {
    // nop
  }
  void actor_exited(abstract_actor* self, uint32_t reason) override {
    m_functor(self, reason);
  }
};

template <class F>
struct functor_attachable<F, 1> : attachable {
  F m_functor;
  functor_attachable(F arg) : m_functor(std::move(arg)) {
    // nop
  }
  void actor_exited(abstract_actor*, uint32_t reason) override {
    m_functor(reason);
  }
};

} // namespace detail
} // namespace caf

#endif // CAF_FUNCTOR_ATTACHABLE_HPP
