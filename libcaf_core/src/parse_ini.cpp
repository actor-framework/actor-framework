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

#include "caf/detail/parse_ini.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "caf/string_algorithms.hpp"

namespace caf {

void detail::parse_ini(std::istream& raw_data, std::ostream& errors,
                       config_consumer consumer) {
  std::string group;
  std::string line;
  size_t ln = 0; // line number
  while (getline(raw_data, line)) {
    ++ln;
    // get begin-of-line (bol) and end-of-line (eol), ignoring whitespaces
    auto eol = find_if_not(line.rbegin(), line.rend(), ::isspace).base();
    auto bol = find_if_not(line.begin(), eol, ::isspace);
    // ignore empty lines and lines starting with ';' (comment)
    if (bol == eol || *bol == ';')
      continue;
    // do we read a group name?
    if (*bol == '[') {
      if (*(eol - 1) != ']') {
        errors << "error in line " << ln << ": missing ] at end of line"
               << std::endl;
      } else {
        group.assign(bol + 1, eol - 1);
      }
      // skip further processing of this line
      continue;
    }
    // do we have a group name yet? (prohibit ungrouped values)
    if (group.empty()) {
      errors << "error in line " << ln << ": value outside of a group"
             << std::endl;
      continue;
    }
    // position of the equal sign
    auto eqs = find(bol, eol, '=');
    if (eqs == eol) {
      errors << "error in line " << ln << ": no '=' found" << std::endl;
      continue;
    }
    if (bol == eqs) {
      errors << "error in line " << ln << ": line starting with '='"
             << std::endl;
      continue;
    }
    if ((eqs + 1) == eol) {
      errors << "error in line " << ln << ": line ends with '='" << std::endl;
      continue;
    }
    // our keys have the format "<group>.<config-name>"
    auto key = group;
    key += '.';
    // ignore any whitespace between config-name and equal sign
    key.insert(key.end(), bol, find_if(bol, eqs, ::isspace));
    // begin-of-value, ignoreing whitespaces after '='
    auto bov = find_if_not(eqs + 1, eol, ::isspace);
    // auto-detect what we are dealing with
    const char* true_str = "true";
    const char* false_str = "false";
    if (std::equal(bov, eol, true_str)) {
      consumer(std::move(key), true);
    } else if (std::equal(bov, eol, false_str)) {
      consumer(std::move(key), false);
    } else if (*bov == '"') {
      // found a string, remove first and last char from string, start escaping
      // string sequence
      auto last_char_backslash = false;
      std::string result = "";
      // skip leading " and iterate up to the trailing "
      ++bov;
      for (; bov+1 != eol; ++bov) {
        if (last_char_backslash) {
          switch (*bov) {
            case 'n':
              result += '\n';
              break;
            default:
              result += *bov;
          }
          last_char_backslash = false;
        } else if (*bov == '\\') {
          last_char_backslash = true;
        } else {
          result += *bov;
        }
      }
      if (last_char_backslash) {
        errors << "error in line " << ln
               << ": trailing quotation mark was escaped" << std::endl;
      }
      consumer(std::move(key), std::move(result));
    } else {
      bool is_neg = *bov == '-';
      if (is_neg && ++bov == eol) {
        errors << "error in line " << ln << ": '-' is not a number"
               << std::endl;
        continue;
      }
      auto set_ival = [&](int base) -> bool {
        char* e;
        int64_t res = std::strtoll(&*bov, &e, base);
        if (e == &*eol) {
          consumer(std::move(key), is_neg ? -res : res);
          return true;
        }
        return false;
      };
      // are we dealing with a hex?
      const char* hex_prefix = "0x";
      if (std::equal(hex_prefix, hex_prefix + 2, bov)) {
        if (! set_ival(16)) {
          errors << "error in line " << ln << ": invalid hex value"
                 << std::endl;
        }
      } else if (all_of(bov, eol, ::isdigit)) {
        // check for base 8 and 10
        if (*bov == '0') {
          if (! set_ival(8)) {
            errors << "error in line " << ln << ": invalid oct value"
                   << std::endl;
          }
        } else {
          if (! set_ival(10)) {
            errors << "error in line " << ln << ": invalid decimal value"
                   << std::endl;
          }
        }
      } else {
        // try to parse a double value
        char* e;
        double res = std::strtod(&*bov, &e);
        if (e == &*eol) {
          consumer(std::move(key), is_neg ? -res : res);
        } else {
            errors << "error in line " << ln << ": can't parse value to double"
                   << std::endl;
        }
      }
    }
  }
}

} // namespace caf
