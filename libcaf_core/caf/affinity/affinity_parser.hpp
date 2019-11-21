/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <set>
#include <string>
#include <vector>

namespace caf {
namespace affinity {

class parser {
public:
  static void parseaffinity(std::string affinitystr,
                            std::vector<std::set<int>>& cores_groups);

private:
  static const char OPENGROUP;
  static const char CLOSEGROUP;
  static const char SETSEPARATOR;
  static const char RANGESEPARATOR;
  static const std::string SPACE;

  static size_t ZERO;
  static bool onlyspaceafter(std::string s, size_t next, size_t& pos=ZERO);

  static bool onlyspace(std::string s, size_t& pos=ZERO);

  static int getsinglenum(std::string s);

  static std::set<int> parseaffinitygroup(std::string s);

  static std::string getaffinitygroup(std::string& affinitystring);
};

} // namespace affinity
} // namespace caf
