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

#include "caf/detail/decorated_tuple.hpp"

namespace caf {
namespace detail {

void* decorated_tuple::mutable_at(size_t pos) {
  CAF_REQUIRE(pos < size());
  return m_decorated->mutable_at(m_mapping[pos]);
}

size_t decorated_tuple::size() const {
  return m_mapping.size();
}

decorated_tuple* decorated_tuple::copy() const {
  return new decorated_tuple(*this);
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

void decorated_tuple::init() {
  CAF_REQUIRE(m_mapping.empty()
              || *(std::max_element(m_mapping.begin(), m_mapping.end()))
                 < static_cast<const pointer&>(m_decorated)->size());
  // calculate type token
  for (size_t i = 0; i < m_mapping.size(); ++i) {
    m_type_token <<= 6;
    m_type_token |= m_decorated->type_nr_at(m_mapping[i]);
  }
}

void decorated_tuple::init(size_t offset) {
  if (offset < m_decorated->size()) {
    size_t i = offset;
    size_t new_size = m_decorated->size() - offset;
    m_mapping.resize(new_size);
    std::generate(m_mapping.begin(), m_mapping.end(), [&] { return i++; });
  }
  init();
}

decorated_tuple::decorated_tuple(pointer d, vector_type&& v)
    : m_decorated(std::move(d)),
      m_mapping(std::move(v)),
      m_type_token(0xFFFFFFFF) {
  init();
}

decorated_tuple::decorated_tuple(pointer d, size_t offset)
    : m_decorated(std::move(d)),
      m_type_token(0xFFFFFFFF) {
  init(offset);
}

const std::string* decorated_tuple::tuple_type_names() const {
  // produced name is equal for all instances
  static std::string result = get_tuple_type_names(*this);
  return &result;
}

} // namespace detail
} // namespace caf
