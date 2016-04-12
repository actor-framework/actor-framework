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

#ifndef CAF_DETAIL_DECORATED_TUPLE_HPP
#define CAF_DETAIL_DECORATED_TUPLE_HPP

#include <vector>
#include <algorithm>

#include "caf/config.hpp"
#include "caf/ref_counted.hpp"

#include "caf/detail/type_list.hpp"

#include "caf/detail/tuple_vals.hpp"
#include "caf/detail/message_data.hpp"

namespace caf {
namespace detail {

class decorated_tuple : public message_data {
public:
  decorated_tuple& operator=(const decorated_tuple&) = delete;

  using vector_type = std::vector<size_t>;

  using message_data::cow_ptr;

  decorated_tuple(cow_ptr&&, vector_type&&);

  // creates a typed subtuple from `d` with mapping `v`
  static cow_ptr make(cow_ptr d, vector_type v);

  void* get_mutable(size_t pos) override;

  void load(size_t pos, deserializer& source) override;

  size_t size() const override;

  cow_ptr copy() const override;

  const void* get(size_t pos) const override;

  uint32_t type_token() const override;

  rtti_pair type(size_t pos) const override;

  void save(size_t pos, serializer& sink) const override;

  std::string stringify(size_t pos) const override;

  inline const cow_ptr& decorated() const {
    return decorated_;
  }

  inline const vector_type& mapping() const {
    return mapping_;
  }

private:
  decorated_tuple(const decorated_tuple&) = default;

  cow_ptr decorated_;
  vector_type mapping_;
  uint32_t type_token_;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_DECORATED_TUPLE_HPP
