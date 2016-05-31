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

#ifndef CAF_TYPED_ACTOR_POINTER_HPP
#define CAF_TYPED_ACTOR_POINTER_HPP

#include "caf/typed_actor_view.hpp"

#include "caf/detail/type_list.hpp"

namespace caf {

template <class... Sigs>
class typed_actor_pointer {
public:
  template <class Supertype>
  typed_actor_pointer(Supertype* selfptr) : view_(selfptr) {
    using namespace caf::detail;
    static_assert(tl_subset_of<
                    type_list<Sigs...>,
                    typename Supertype::signatures
                  >::value,
                  "cannot create a pointer view to an unrelated actor type");
  }

  typed_actor_pointer(const std::nullptr_t&) : view_(nullptr) {
    // nop
  }

  typed_actor_view<Sigs...>* operator->() {
    return &view_;
  }

private:
  typed_actor_view<Sigs...> view_;
};

} // namespace caf

#endif // CAF_TYPED_ACTOR_POINTER_HPP
