/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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

#include "caf/message.hpp"
#include "caf/actor_addr.hpp"
#include "caf/attachable.hpp"
#include "caf/abstract_actor.hpp"

namespace caf {
namespace detail {

class merged_tuple : public message_data {
public:
  // -- member types -----------------------------------------------------------

  using message_data::cow_ptr;

  using data_type = std::vector<cow_ptr>;

  using mapping_type = std::vector<std::pair<size_t, size_t>>;

  // -- constructors, destructors, and assignment operators --------------------

  static cow_ptr make(message x, message y);

  merged_tuple(data_type xs, mapping_type ys);

  merged_tuple& operator=(const merged_tuple&) = delete;

  // -- overridden observers of message_data -----------------------------------

  merged_tuple* copy() const override;

  // -- overridden modifiers of type_erased_tuple ------------------------------

  void* get_mutable(size_t pos) override;

  error load(size_t pos, deserializer& source) override;

  // -- overridden observers of type_erased_tuple ------------------------------

  size_t size() const noexcept override;

  uint32_t type_token() const noexcept override;

  rtti_pair type(size_t pos) const noexcept override;

  const void* get(size_t pos) const noexcept override;

  std::string stringify(size_t pos) const override;

  type_erased_value_ptr copy(size_t pos) const override;

  error save(size_t pos, serializer& sink) const override;

  // -- observers --------------------------------------------------------------

  const mapping_type& mapping() const;

private:
  // -- constructors, destructors, and assignment operators --------------------

  merged_tuple(const merged_tuple&) = default;

  // -- data members -----------------------------------------------------------

  data_type data_;
  uint32_t type_token_;
  mapping_type mapping_;
};

} // namespace detail
} // namespace caf

