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

#include "caf/message.hpp"

#include "caf/detail/make_counted.hpp"
#include "caf/detail/concatenated_tuple.hpp"

#include <numeric>

namespace caf {
namespace detail {

auto concatenated_tuple::make(std::initializer_list<pointer> xs) -> pointer {
  auto result = detail::make_counted<concatenated_tuple>();
  auto append = [&](const pointer& ptr) {
    auto dptr = dynamic_cast<const concatenated_tuple*>(ptr.get());
    if (dptr) {
      auto& vec = dptr->m_data;
      result->m_data.insert(result->m_data.end(), vec.begin(), vec.end());
    } else {
      result->m_data.push_back(std::move(ptr));
    }
  };
  for (auto& x : xs) {
    if (x) {
      append(x);
    }
  }
  result->init();
  return pointer{std::move(result)};
}

void* concatenated_tuple::mutable_at(size_t pos) {
  CAF_REQUIRE(pos < size());
  auto selected = select(pos);
  return selected.first->mutable_at(selected.second);
}

size_t concatenated_tuple::size() const {
  return m_size;
}

concatenated_tuple* concatenated_tuple::copy() const {
  return new concatenated_tuple(*this);
}

const void* concatenated_tuple::at(size_t pos) const {
  CAF_REQUIRE(pos < size());
  auto selected = select(pos);
  return selected.first->at(selected.second);
}

bool concatenated_tuple::match_element(size_t pos, uint16_t typenr,
                                       const std::type_info* rtti) const {
  auto selected = select(pos);
  return selected.first->match_element(selected.second, typenr, rtti);
}

uint32_t concatenated_tuple::type_token() const {
  return m_type_token;
}

const char* concatenated_tuple::uniform_name_at(size_t pos) const {
  CAF_REQUIRE(pos < size());
  auto selected = select(pos);
  return selected.first->uniform_name_at(selected.second);
}

uint16_t concatenated_tuple::type_nr_at(size_t pos) const {
  CAF_REQUIRE(pos < size());
  auto selected = select(pos);
  return selected.first->type_nr_at(selected.second);
}

std::pair<message_data*, size_t> concatenated_tuple::select(size_t pos) const {
  auto idx = pos;
  for (auto& m : m_data) {
    auto s = m->size();
    if (idx >= s) {
      idx -= s;
    } else {
      return {m.get(), idx};
    }
  }
  throw std::out_of_range("out of range: concatenated_tuple::select");
}

void concatenated_tuple::init() {
  m_type_token = make_type_token();
  for (auto& m : m_data) {
    for (size_t i = 0; i < m->size(); ++i) {
      m_type_token = add_to_type_token(m_type_token, m->type_nr_at(i));
    }
  }
  auto acc_size = [](size_t tmp, const pointer& val) {
    return tmp + val->size();
  };
  m_size = std::accumulate(m_data.begin(), m_data.end(), size_t{0}, acc_size);
}

} // namespace detail
} // namespace caf
