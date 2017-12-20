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

#ifndef CAF_STAT_STREAM_HPP
#define CAF_STAT_STREAM_HPP

#include <algorithm>
#include <cstdint>
#include <string>
#include <limits>

namespace caf {
namespace instrumentation {

/// Compute statistical properties on a stream of measures.
class stat_stream {
public:
  void record(double value);
  double average() const;
  double variance() const;
  double stddev() const;
  void combine(const stat_stream& rhs);
  std::string to_string() const;

  friend void swap(stat_stream&, stat_stream&);

  bool empty() const {
    return _n == 0;
  }

  int32_t count() const {
    return _n;
  }

  double min() const {
    return _min;
  }

  double max() const {
    return _max;
  }

private:
  uint32_t _n = 0;
  double _min = std::numeric_limits<double>::max();
  double _max = std::numeric_limits<double>::lowest();
  double _m1 = 0.0;
  double _m2 = 0.0;
};

inline void swap(stat_stream& a, stat_stream& b)
{
  stat_stream tmp = a;
  a = b;
  b = tmp;
}

} // namespace instrumentation
} // namespace caf

#endif // CAF_STAT_STREAM_HPP
