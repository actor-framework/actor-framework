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

#include "caf/affinity/affinity_parser.hpp"

#include <iostream>
#include <set>
#include <string>

#include "caf/actor_system_config.hpp"

namespace caf {
namespace affinity {

const char parser::OPENGROUP = '<';
const char parser::CLOSEGROUP = '>';
const char parser::SETSEPARATOR = ',';
const char parser::RANGESEPARATOR = '-';
const std::string parser::SPACE = " \n\r\t";

void parser::parseaffinity(std::string affinitystr,
                           std::vector<std::set<int>>& core_groups) {
  std::vector<std::string> errors(0);
  core_groups.clear();

  while (!affinitystr.empty()) {
    try {
      std::set<int> Set = parseaffinitygroup(getaffinitygroup(affinitystr));
      if (!Set.empty())
        core_groups.push_back(move(Set));
    } catch (std::logic_error& e) {
      errors.push_back(std::string{e.what()} + "\n");
    }
  }

  // manage errors
  if (!errors.empty()) {
    core_groups.clear();
    std::string err_str{""};
    for (const std::string error : errors) {
      err_str += error;
    }

    // TODO: improve errors handling
    std::cerr << "[WARNING] Affinity string error skip affinity configuration."
              << std::endl
              << err_str << std::endl;
  }
}

std::string parser::getaffinitygroup(std::string& affinitystring) {
  std::string scopy{affinitystring};

  // do not consider string with only spaces
  size_t pos1;
  if (onlyspace(affinitystring, pos1)) {
    affinitystring.clear();
    return "";
  }
  // find open < as first symbol
  size_t pos_open = affinitystring.find_first_of(OPENGROUP);
  if (pos_open == std::string::npos || pos_open != pos1) {
    affinitystring.clear();
    throw std::logic_error("open angular bracket not found before \"" + scopy
                           + "\"");
  }
  // find closed > and there are not other open > before
  size_t pos_close = affinitystring.find_first_of(CLOSEGROUP, pos_open + 1);
  size_t other_open = affinitystring.find_first_of(OPENGROUP, pos_open + 1);
  if (pos_close == std::string::npos
      || (other_open != std::string::npos && other_open < pos_close)) {
    affinitystring.clear();
    throw std::logic_error("closed angular bracket not found after \"" + scopy
                           + "\"");
  }
  size_t pos_start = pos_open + 1;
  size_t pos_end = pos_close - 1;
  // check that the group is not empty
  if (pos_start <= pos_end) {
    const std::string s =
      affinitystring.substr(pos_start, pos_end - pos_start + 1);
    if (!onlyspace(s)) {
      // the group is not empty
      affinitystring = affinitystring.substr(pos_close + 1);
      return s;
    }
  }
  // the group is empty
  affinitystring = affinitystring.substr(pos_close + 1);
  throw std::logic_error("group is empty on \"" + scopy + "\"");
}

std::set<int> parser::parseaffinitygroup(std::string s) {
  std::set<int> Set;
  const std::string scopy{s};
  const std::string SEPARATOR{RANGESEPARATOR, SETSEPARATOR};
  try {
    std::stringstream ss(s);
    std::string el;
    while (std::getline(ss, el, SETSEPARATOR)) {
      size_t pos1 = el.find_first_of(RANGESEPARATOR);
      if (pos1 == std::string::npos) {
        // OK: this is a single value
        int val = getsinglenum(el);
        Set.insert(val);
      } else {
        if (pos1 > 0 && pos1 < el.size()) {
          std::string left_str = el.substr(0, pos1);
          std::string right_str = el.substr(pos1 + 1);
          int left = getsinglenum(left_str);
          int right = getsinglenum(right_str);
          for (int k = left; k <= right; ++k)
            Set.insert(k);
        } else {
          throw std::invalid_argument("not a range");
        }
      }
    }
    return Set;
  } catch (std::invalid_argument& e) {
    throw std::logic_error("invalid value: " + std::string{e.what()}
                           + " into the group \"" + scopy + "\"");
  }
  return Set;
}

size_t parser::ZERO = 0;
bool parser::onlyspaceafter(std::string s, size_t next, size_t& pos) {
  pos = s.find_first_not_of(SPACE, next);
  return pos == std::string::npos;
}
bool parser::onlyspace(std::string s, size_t& pos) {
  return onlyspaceafter(s, 0, pos);
}

int parser::getsinglenum(std::string s) {
  size_t next;
  int val;
  try {
    val = std::stoi(s, &next);
  } catch (std::invalid_argument& e) {
    throw std::invalid_argument("\"" + s + "\"" + " is not a number");
  }
  if (next != s.size() && !onlyspaceafter(s, next))
    throw std::invalid_argument("\"" + s + "\"" + " is not a number");
  if (val < 0)
    throw std::invalid_argument("\"" + s + "\"" + "is negative");
  return val;
}

} // namespace affinity
} // namespace caf
