/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#ifndef CAF_PAIR_HASH_HPP
#define CAF_PAIR_HASH_HPP

#include <utility>

namespace std {

// stolen from boost::hash_combine
template<class T>
inline void hash_combine(std::size_t &seed, T const &v) {
  seed ^= std::hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template<typename A, typename B>
struct hash<std::pair<A, B>> {
  size_t operator()(const std::pair<A, B> &p) const {
    uint64_t hash = std::hash<A>()(p.first);
    hash_combine(hash, p.second);
    return hash;
  }
};

} // namespace std

#endif // CAF_PAIR_HASH_HPP
