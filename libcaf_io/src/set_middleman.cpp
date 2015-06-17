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

#include "caf/io/set_middleman.hpp"

namespace caf {
namespace io {

void set_middleman(network::multiplexer* multiplexer_ptr) {
  middleman::backend_pointer uptr;
  uptr.reset(multiplexer_ptr);
  auto fac = [&uptr] { return std::move(uptr); };
  auto mm = new middleman(fac);
  auto getter = [mm] { return mm; };
  auto sid = detail::singletons::middleman_plugin_id;
  auto res = detail::singletons::get_plugin_singleton(sid, getter);
  if (res != mm) {
    delete mm;
    throw std::logic_error("middleman already defined");
  }
}

} // namespace io
} // namespace caf
