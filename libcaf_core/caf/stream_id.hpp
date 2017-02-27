/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#ifndef CAF_STREAM_ID_HPP
#define CAF_STREAM_ID_HPP

#include <cstdint>

#include "caf/meta/type_name.hpp"
#include "caf/actor_control_block.hpp"

namespace caf {

struct stream_id {
  strong_actor_ptr origin;
  uint64_t nr;
};

inline bool operator==(const stream_id& x, const stream_id& y) {
  return x.origin == y.origin && x.nr == y.nr;
}

inline bool operator<(const stream_id& x, const stream_id& y) {
  return x.origin == y.origin ? x.nr < y.nr : x.origin < y.origin;
}

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, stream_id& x) {
  return f(meta::type_name("stream_id"), x.origin, x.nr);
}

} // namespace caf

namespace std {
template <>
struct hash<caf::stream_id> {
  size_t operator()(const caf::stream_id& x) const {
    auto tmp = reinterpret_cast<ptrdiff_t>(x.origin.get())
               ^ static_cast<ptrdiff_t>(x.nr);
    return static_cast<size_t>(tmp);
  }
};
} // namespace std


#endif // CAF_STREAM_ID_HPP
