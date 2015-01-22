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

#ifndef CAF_DETAIL_DECORATED_TUPLE_HPP
#define CAF_DETAIL_DECORATED_TUPLE_HPP

#include <vector>
#include <algorithm>

#include "caf/config.hpp"
#include "caf/ref_counted.hpp"
#include "caf/uniform_type_info.hpp"

#include "caf/detail/type_list.hpp"

#include "caf/detail/tuple_vals.hpp"
#include "caf/detail/message_data.hpp"

namespace caf {
namespace detail {

class decorated_tuple : public message_data {

  using super = message_data;

  decorated_tuple& operator=(const decorated_tuple&) = delete;

 public:

  using vector_type = std::vector<size_t>;

  using pointer = message_data::ptr;

  // creates a typed subtuple from `d` with mapping `v`
  static inline pointer create(pointer d, vector_type v) {
    return pointer{new decorated_tuple(std::move(d), std::move(v))};
  }

  // creates a typed subtuple from `d` with an offset
  static inline pointer create(pointer d, size_t offset) {
    return pointer{new decorated_tuple(std::move(d), offset)};
  }

  void* mutable_at(size_t pos) override;

  size_t size() const override;

  decorated_tuple* copy() const override;

  const void* at(size_t pos) const override;

  const uniform_type_info* type_at(size_t pos) const override;

  const std::string* tuple_type_names() const override;

 private:

  pointer m_decorated;
  vector_type m_mapping;

  void init();

  void init(size_t);

  decorated_tuple(pointer, size_t);

  decorated_tuple(pointer, vector_type&&);

  decorated_tuple(const decorated_tuple&) = default;

};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_DECORATED_TUPLE_HPP
