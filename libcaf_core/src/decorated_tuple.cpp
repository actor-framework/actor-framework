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

#include "caf/detail/decorated_tuple.hpp"

#include "caf/make_counted.hpp"

namespace caf {
namespace detail {

decorated_tuple::cow_ptr decorated_tuple::make(cow_ptr d, vector_type v) {
  auto ptr = dynamic_cast<const decorated_tuple*>(d.get());
  if (ptr) {
    d = ptr->decorated();
    auto& pmap = ptr->mapping();
    for (size_t i = 0; i < v.size(); ++i) {
      v[i] = pmap[v[i]];
    }
  }
  return make_counted<decorated_tuple>(std::move(d), std::move(v));
}

void* decorated_tuple::mutable_at(size_t pos) {
  CAF_REQUIRE(pos < size());
  return m_decorated->mutable_at(m_mapping[pos]);
}

size_t decorated_tuple::size() const {
  return m_mapping.size();
}

message_data::cow_ptr decorated_tuple::copy() const {
  return cow_ptr(new decorated_tuple(*this), false);
}

const void* decorated_tuple::at(size_t pos) const {
  CAF_REQUIRE(pos < size());
  return m_decorated->at(m_mapping[pos]);
}

bool decorated_tuple::match_element(size_t pos, uint16_t typenr,
                                    const std::type_info* rtti) const {
  return m_decorated->match_element(m_mapping[pos], typenr, rtti);
}

uint32_t decorated_tuple::type_token() const {
  return m_type_token;
}

const char* decorated_tuple::uniform_name_at(size_t pos) const {
  return m_decorated->uniform_name_at(m_mapping[pos]);
}

uint16_t decorated_tuple::type_nr_at(size_t pos) const {
  return m_decorated->type_nr_at(m_mapping[pos]);
}

decorated_tuple::decorated_tuple(cow_ptr&& d, vector_type&& v)
    : m_decorated(std::move(d)),
      m_mapping(std::move(v)),
      m_type_token(0xFFFFFFFF) {
  CAF_REQUIRE(m_mapping.empty()
              || *(std::max_element(m_mapping.begin(), m_mapping.end()))
                 < static_cast<const cow_ptr&>(m_decorated)->size());
  // calculate type token
  for (size_t i = 0; i < m_mapping.size(); ++i) {
    m_type_token <<= 6;
    m_type_token |= m_decorated->type_nr_at(m_mapping[i]);
  }
}

} // namespace detail
} // namespace caf
