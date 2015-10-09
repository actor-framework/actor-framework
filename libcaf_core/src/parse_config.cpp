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

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "caf/actor.hpp"
#include "caf/atom.hpp"
#include "caf/detail/optional_message_visitor.hpp"
#include "caf/detail/parse_ini.hpp"
#include "caf/experimental/whereis.hpp"

namespace caf {

void parse_config(const std::string& file_name,
                  config_format cf) {
  if (cf == config_format::auto_detect) {
    // try to detect file format according to file extension
    if (file_name.size() < 5) {
      std::cerr << "filename is to short" << std::endl;
    } else if (file_name.compare(file_name.size() - 4, 4, ".ini")) {
      parse_config(file_name, config_format::ini);
    } else {
      std::cerr << "unknown config file format" << std::endl;
    }
    return;
  }

  std::ifstream raw_data(file_name);
  std::stringstream error_stream;

  struct consumer {
    actor config_server_;

    consumer() {
      config_server_ = experimental::whereis(atom("ConfigServ"));
    }

    consumer(const consumer&) = default;
    consumer& operator=(const consumer&) = default;
    void operator()(const std::string&, config_value) const {
      // send message to config server
    }
  };
  consumer cons;

  switch (cf) {
    case config_format::ini:
      detail::parse_ini(raw_data, error_stream, cons);
      break;
    default:
      std::cerr << "no other format is supported" << std::endl;
      break;
  }
  raw_data.close();
}
} // namespace caf
