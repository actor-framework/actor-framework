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

#include <cmath>
#include <array>
#include <string>
#include <cstdint>

namespace caf {
namespace instrumentation {

/// Compute statistical properties on a stream of measures.
class stat_stream {
  static const int slice_count = 8;

public:
  void record(double value) {
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
    double delta_n2 = delta_n * delta_n;
    double term1 = delta * delta_n * n1;
    _m1 += delta_n;
    _m4 += term1 * delta_n2 * (_n*_n - 3*_n + 3) + 6 * delta_n2 * _m2 - 4 * delta_n * _m3;
    _m3 += term1 * delta_n * (_n - 2) - 3 * delta_n * _m2;
    _m2 += term1;
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

  double average() const {
    return (_n > 0) ? _m1 : 0.0;
  }

  double variance() const {
    return (_n > 1) ? _m2/(_n - 1) : 0.0;
  }

  double stddev() const {
    return sqrt(variance());
  }

  void combine(const stat_stream& rhs) {
    stat_stream combined;

    combined._n = _n + rhs._n;
    if (combined._n == 0)
      return;

    double delta = rhs._m1 - _m1;
    double delta2 = delta*delta;
    double delta3 = delta*delta2;
    double delta4 = delta2*delta2;

    combined._m1 = (_n*_m1 + rhs._n*rhs._m1) / combined._n;

    combined._m2 = _m2 + rhs._m2 +
                   delta2 * _n * rhs._n / combined._n;

    combined._m3 = _m3 + rhs._m3 +
                   delta3 * _n * rhs._n * (_n - rhs._n)/(combined._n*combined._n);
    combined._m3 += 3.0*delta * (_n*rhs._m2 - rhs._n*_m2) / combined._n;

    combined._m4 = _m4 + rhs._m4 + delta4*_n*rhs._n * (_n*_n - _n*rhs._n + rhs._n*rhs._n) /
                                   (combined._n*combined._n*combined._n);
    combined._m4 += 6.0*delta2 * (_n*_n*rhs._m2 + rhs._n*rhs._n*_m2)/(combined._n*combined._n) +
                    4.0*delta*(_n*rhs._m3 - rhs._n*_m3) / combined._n;

    _n = combined._n;
    if (rhs._min < _min)
      _min = rhs._min;
    if (rhs._max > _max)
      _max = rhs._max;
    _m1 = combined._m1;
    _m2 = combined._m2;
    _m3 = combined._m3;
    _m4 = combined._m4;
  }

  std::string to_string() const {
    std::string res;
    res += "Cnt:" + std::to_string(_n);
    res += " Min:" + std::to_string(_min);
    res += " Max:" + std::to_string(_max);
    res += " Avg:" + std::to_string(average());
    res += " Stddev:" + std::to_string(stddev());
    return res;
  }

private:
  uint32_t _n = 0;
  double _min = 1e10;
  double _max = -1e10;
  double _m1 = 0.0; // M1
  double _m2 = 0.0; // M2
  double _m3 = 0.0; // M3
  double _m4 = 0.0; // M4
};

} // namespace instrumentation
} // namespace caf

#endif // CAF_STAT_STREAM_HPP
