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

#ifndef CAF_METRIC_HPP
#define CAF_METRIC_HPP

#include "caf/instrumentation/callsite_stats.hpp"

#include <cmath>
#include <array>
#include <string>
#include <cstdint>
#include <utility>

namespace caf {
namespace instrumentation {

enum class metric_type {
  pre_behavior,
  broker_forward
};

struct metric_key {
  metric_type type;
  std::string actortype;
  std::string callsite;

  bool operator==(const metric_key& other) const {
    return type == other.type
           && actortype == other.actortype
           && callsite == other.callsite;
  }
};

struct metric {
  metric(metric_type type, const std::string& actortype, const std::string& callsite, const callsite_stats& value)
    : key{type, actortype, callsite},
      value(value)
  {}

  void combine(const metric& rhs) {
    value.combine(rhs.value);
  }

  metric_key key;
  callsite_stats value;
};

} // namespace instrumentation
} // namespace caf

namespace std {

// stolen from boost::hash_combine
template <class T>
inline void hash_combine(std::size_t& seed, T const& v)
{
  seed ^= std::hash<T>()(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
}

template <>
struct hash<caf::instrumentation::metric_key>
{
  size_t operator()(const caf::instrumentation::metric_key& k) const {
    auto hash = static_cast<size_t>(k.type);
    hash_combine(hash, k.actortype);
    hash_combine(hash, k.callsite);
    return hash;
  }
};

} // namespace std

#endif // CAF_METRIC_HPP
