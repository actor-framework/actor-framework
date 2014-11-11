/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#include <future>

#include "caf/actor_cast.hpp"

#include "caf/abstract_actor.hpp"
#include "caf/detail/singletons.hpp"
#include "caf/detail/actor_registry.hpp"

#include "caf/io/publish.hpp"
#include "caf/io/middleman.hpp"
#include "caf/io/basp_broker.hpp"

namespace caf {
namespace io {

void publish_impl(abstract_actor_ptr whom, uint16_t port, const char* in) {
  using namespace detail;
  auto mm = middleman::instance();
  std::promise<bool> res;
  mm->run_later([&] {
    auto bro = mm->get_named_broker<basp_broker>(atom("_BASP"));
    try {
      auto hdl = mm->backend().add_tcp_doorman(bro.get(), port, in);
      bro->add_published_actor(hdl, whom, port);
      mm->notify<hook::actor_published>(whom->address(), port);
      res.set_value(true);
    }
    catch (...) {
      res.set_exception(std::current_exception());
    }
  });
  // block caller and re-throw exception here in case of an error
  res.get_future().get();
}

} // namespace io
} // namespace caf
