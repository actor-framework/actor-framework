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

#include "caf/parse_config.hpp"

#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>

#include "caf/atom.hpp"
#include "caf/send.hpp"
#include "caf/actor.hpp"

#include "caf/experimental/whereis.hpp"

#include "caf/detail/parse_ini.hpp"
#include "caf/detail/optional_message_visitor.hpp"

namespace caf {

class message_visitor : public static_visitor<message> {
public:
  template <class T>
  message operator()(T& value) const {
    return make_message(std::move(value));
  }
};

void parse_config(std::istream& input, config_format format,
                  maybe<std::ostream&> errors) {
  if (! input)
    return;
  auto cs = experimental::whereis(atom("ConfigServ"));
  auto consume = [&](std::string key, config_value value) {
    message_visitor mv;
    anon_send(cs, put_atom::value, std::move(key), apply_visitor(mv, value));
  };
  switch (format) {
    case config_format::ini:
      detail::parse_ini(input, consume, errors);
  }
}

void parse_config(const std::string& file_name,
                  maybe<config_format> format,
                  maybe<std::ostream&> errors) {
  if (! format) {
    // try to detect file format according to file extension
    if (file_name.size() < 5)
      std::cerr << "filename is to short" << std::endl;
    else if (file_name.compare(file_name.size() - 4, 4, ".ini"))
      parse_config(file_name, config_format::ini, errors);
    else if (errors)
      *errors << "error: unknown config file format" << std::endl;
    return;
  }
  std::ifstream input{file_name};
  if (! input) {
    if (errors)
      *errors << "error: unable to open " << file_name << std::endl;
    return;
  }
  parse_config(input, *format, errors);
}

} // namespace caf
