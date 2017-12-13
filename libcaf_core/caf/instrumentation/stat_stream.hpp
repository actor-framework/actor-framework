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
///  _slices will store the count of measures in each of the following buckets:
///    _slices[0] for measures in [0;Slice1)
///    _slices[1] for measures in [Slice1;Slice2)
///    ...
///    _slices[6] for measures in [Slice6;Slice7)
///    _slices[7] for measures in [Slice7;+infinity)
template<typename T, T Slice1, T Slice2, T Slice3, T Slice4, T Slice5, T Slice6, T Slice7>
class stat_stream {
  static const int slice_count = 8;

public:
  void record(T value)
  {
    // Counts
    _count++;
    if (value >= Slice7)
      _slices[7]++;
    else if (value >= Slice6)
      _slices[6]++;
    else if (value >= Slice5)
      _slices[5]++;
    else if (value >= Slice4)
      _slices[4]++;
    else if (value >= Slice3)
      _slices[3]++;
    else if (value >= Slice2)
      _slices[2]++;
    else if (value >= Slice1)
      _slices[1]++;
    else
      _slices[0]++;

    // Statistical properties
    // Computed with numerical stability thanks to https://www.johndcook.com/blog/standard_deviation/
    double v = value;
    if (_count == 1)
    {
      _oldM = _newM = v;
      _oldS = 0.0;
    }
    else
    {
      _newM = _oldM + (v - _oldM)/_count;
      _newS = _oldS + (v - _oldM)*(v - _newM);
      _oldM = _newM;
      _oldS = _newS;
    }
  }

  double average() const
  {
    return (_count > 0) ? _newM : 0.0;
  }

  double variance() const
  {
    return (_count > 1) ? _newS/(_count - 1) : 0.0;
  }

  double stddev() const
  {
    return sqrt(variance());
  }

  std::string to_string() const
  {
    std::string res;
    res += "Cnt:" + std::to_string(_count);
    res += " Slices:";
    for (int i = 0; i < slice_count; ++i)
    {
      res += std::to_string(_slices[i]);
      if (i < slice_count - 1)
      {
        res += "|";
      }
    }
    res += " Avg:" + std::to_string(average());
    res += " Stddev:" + std::to_string(stddev());
    return res;
  }

private:
  uint32_t _count = 0;
  std::array<uint32_t, slice_count> _slices = {0, 0, 0, 0, 0, 0, 0, 0};
  double _oldM = 0.0;
  double _newM = 0.0;
  double _oldS = 0.0;
  double _newS = 0.0;
};

} // namespace instrumentation
} // namespace caf

#endif // CAF_STAT_STREAM_HPP
