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

#ifndef CAF_DETAIL_CONCATENATED_TUPLE_HPP
#define CAF_DETAIL_CONCATENATED_TUPLE_HPP

#include <vector>
#include <algorithm>

#include "caf/detail/decorated_tuple.hpp"

namespace caf {
namespace detail {

class concatenated_tuple : public message_data {
public:
  concatenated_tuple& operator=(const concatenated_tuple&) = delete;

  using message_data::cow_ptr;

  using vector_type = std::vector<cow_ptr>;

  static cow_ptr make(std::initializer_list<cow_ptr> xs);

  void* mutable_at(size_t pos) override;

  size_t size() const override;

  cow_ptr copy() const override;

  const void* at(size_t pos) const override;

  bool match_element(size_t pos, uint16_t typenr,
                     const std::type_info* rtti) const override;

  uint32_t type_token() const override;

  const char* uniform_name_at(size_t pos) const override;

  uint16_t type_nr_at(size_t pos) const override;

  concatenated_tuple(std::initializer_list<cow_ptr> xs);

  concatenated_tuple(const concatenated_tuple&) = default;

  std::pair<message_data*, size_t> select(size_t pos) const;

private:
  vector_type data_;
  uint32_t type_token_;
  size_t size_;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_CONCATENATED_TUPLE_HPP

