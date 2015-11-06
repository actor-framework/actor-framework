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

#include <limits>

#include "caf/binary_serializer.hpp"
#include "caf/detail/network_order.hpp"

namespace caf {

namespace {

template <class T>
void apply_int(binary_serializer& bs, T x) {
  auto y = detail::to_network_order(x);
  bs.apply_raw(sizeof(T), &y);
}

} // namespace <anonmyous>

void binary_serializer::begin_object(uint16_t& nr, std::string& name) {
  apply(nr);
  if (nr == 0)
    apply(name);
}

void binary_serializer::end_object() {
  // nop
}

void binary_serializer::begin_sequence(size_t& list_size) {
  auto s = static_cast<uint32_t>(list_size);
  apply(s);
}

void binary_serializer::end_sequence() {
  // nop
}

void binary_serializer::apply_raw(size_t num_bytes, void* data) {
  out_(reinterpret_cast<char*>(data), num_bytes);
}

void binary_serializer::apply_builtin(builtin type, void* val) {
  CAF_ASSERT(val != nullptr);
  switch (type) {
    case i8_v:
    case u8_v:
      apply_raw(sizeof(uint8_t), val);
      break;
    case i16_v:
    case u16_v:
      apply_int(*this, *reinterpret_cast<uint16_t*>(val));
      break;
    case i32_v:
    case u32_v:
      apply_int(*this, *reinterpret_cast<uint32_t*>(val));
      break;
    case i64_v:
    case u64_v:
      apply_int(*this, *reinterpret_cast<uint64_t*>(val));
      break;
    case float_v:
      apply_int(*this, detail::pack754(*reinterpret_cast<float*>(val)));
      break;
    case double_v:
      apply_int(*this, detail::pack754(*reinterpret_cast<double*>(val)));
      break;
    case ldouble_v: {
      // the IEEE-754 conversion does not work for long double
      // => fall back to string serialization (event though it sucks)
      std::ostringstream oss;
      oss << std::setprecision(std::numeric_limits<long double>::digits)
          << *reinterpret_cast<long double*>(val);
      auto tmp = oss.str();
      auto s = static_cast<uint32_t>(tmp.size());
      apply(s);
      apply_raw(tmp.size(), const_cast<char*>(tmp.c_str()));
      break;
    }
    case string8_v: {
      auto str = reinterpret_cast<std::string*>(val);
      auto s = static_cast<uint32_t>(str->size());
      apply(s);
      apply_raw(str->size(), const_cast<char*>(str->c_str()));
      break;
    }
    case string16_v: {
      auto& str = *reinterpret_cast<std::u16string*>(val);
      auto s = static_cast<uint32_t>(str.size());
      apply(s);
      for (auto c : str) {
        // the standard does not guarantee that char16_t is exactly 16 bits...
        auto tmp = static_cast<uint16_t>(c);
        apply_raw(sizeof(tmp), &tmp);
      }
      break;
    }
    case string32_v: {
      auto& str = *reinterpret_cast<std::u32string*>(val);
      auto s = static_cast<uint32_t>(str.size());
      apply(s);
      for (auto c : str) {
        // the standard does not guarantee that char32_t is exactly 32 bits...
        auto tmp = static_cast<uint32_t>(c);
        apply_raw(sizeof(tmp), &tmp);
      }
      break;
    }
  }
}

} // namespace caf
