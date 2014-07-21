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

#ifndef CAF_DETAIL_TUPLE_DUMMY_HPP
#define CAF_DETAIL_TUPLE_DUMMY_HPP

#include "caf/fwd.hpp"

#include "caf/detail/type_list.hpp"

#include "caf/detail/message_iterator.hpp"

namespace caf {
namespace detail {

struct tuple_dummy {
  using types = detail::empty_type_list;
  using const_iterator = message_iterator<tuple_dummy>;
  inline size_t size() const { return 0; }
  inline void* mutable_at(size_t) { return nullptr; }
  inline const void* at(size_t) const { return nullptr; }
  inline const uniform_type_info* type_at(size_t) const { return nullptr; }
  inline const std::type_info* type_token() const {
    return &typeid(detail::empty_type_list);
  }
  inline bool dynamically_typed() const { return false; }
  inline const_iterator begin() const {
    return {this};
  }
  inline const_iterator end() const {
    return {this, 0};
  }

};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_TUPLE_DUMMY_HPP
