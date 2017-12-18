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

#include "caf/instrumentation/stat_stream.hpp"

#include <cmath>

namespace caf {
namespace instrumentation {

void stat_stream::record(double value) {
  auto n1 = _n;
  _n++;
  if (value < _min)
    _min = value;
  if (value > _max)
    _max = value;

  // Statistical properties
  // See https://www.johndcook.com/blog/skewness_kurtosis/
  double delta = value - _m1;
  double delta_n = delta / _n;
  double term1 = delta * delta_n * n1;
  _m1 += delta_n;
  _m2 += term1;
}

double stat_stream::average() const {
  return (_n > 0) ? _m1 : 0.0;
}

double stat_stream::variance() const {
  return (_n > 1) ? _m2/(_n - 1) : 0.0;
}

double stat_stream::stddev() const {
  return sqrt(variance());
}

void stat_stream::combine(const stat_stream& rhs) {
  stat_stream combined;

  combined._n = _n + rhs._n;
  if (combined._n == 0)
    return;

  double delta = rhs._m1 - _m1;
  double delta2 = delta*delta;

  combined._m1 = (_n*_m1 + rhs._n*rhs._m1) / combined._n;

  combined._m2 = _m2 + rhs._m2 +
                 delta2 * _n * rhs._n / combined._n;

  _n = combined._n;
  if (rhs._min < _min)
    _min = rhs._min;
  if (rhs._max > _max)
    _max = rhs._max;
  _m1 = combined._m1;
  _m2 = combined._m2;
}

std::string stat_stream::to_string() const {
  std::string res;
  res += "Cnt:" + std::to_string(_n);
  if (_n > 0) {
    res += " Min:" + std::to_string(_min);
    res += " Max:" + std::to_string(_max);
    res += " Avg:" + std::to_string(average());
    res += " Stddev:" + std::to_string(stddev());
  }
  return res;
}

} // namespace instrumentation
} // namespace caf
