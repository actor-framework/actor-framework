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

#ifndef CAF_DETAIL_MERGED_TUPLE_HPP
#define CAF_DETAIL_MERGED_TUPLE_HPP

#include "caf/message.hpp"
#include "caf/actor_addr.hpp"
#include "caf/attachable.hpp"
#include "caf/abstract_actor.hpp"

namespace caf {
namespace detail {

class merged_tuple : public message_data {
public:
  merged_tuple& operator=(const merged_tuple&) = delete;

  using mapping_type = std::vector<std::pair<size_t, size_t>>;

  using message_data::cow_ptr;

  using data_type = std::vector<cow_ptr>;

  merged_tuple(data_type xs, mapping_type ys);

  // creates a typed subtuple from `d` with mapping `v`
  static cow_ptr make(message x, message y);

  void* mutable_at(size_t pos) override;

  void serialize_at(deserializer& source, size_t pos) override;

  size_t size() const override;

  cow_ptr copy() const override;

  const void* at(size_t pos) const override;

  bool compare_at(size_t pos, const element_rtti& rtti,
                  const void* x) const override;

  bool match_element(size_t pos, uint16_t typenr,
                     const std::type_info* rtti) const override;

  uint32_t type_token() const override;

  element_rtti type_at(size_t pos) const override;

  void serialize_at(serializer& sink, size_t pos) const override;

  std::string stringify_at(size_t pos) const override;

  const mapping_type& mapping() const;

private:
  merged_tuple(const merged_tuple&) = default;

  data_type data_;
  uint32_t type_token_;
  mapping_type mapping_;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_MERGED_TUPLE_HPP
