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

#include "caf/io/unpublish.hpp"

#include "caf/send.hpp"
#include "caf/scoped_actor.hpp"

#include "caf/io/middleman.hpp"
#include "caf/io/unpublish.hpp"
#include "caf/io/basp_broker.hpp"

namespace caf {
namespace io {

void unpublish_impl(abstract_actor_ptr whom, uint16_t port, bool block_caller) {
  auto mm = middleman::instance();
  if (block_caller) {
    scoped_actor self;
    mm->run_later([&] {
      auto bro = mm->get_named_broker<basp_broker>(atom("_BASP"));
      bro->remove_published_actor(whom, port);
      anon_send(self, atom("done"));
    });
    self->receive(
      on(atom("done")) >> [] {
        // ok, basp_broker is done
      }
    );
  } else {
    mm->run_later([whom, port, mm] {
      auto bro = mm->get_named_broker<basp_broker>(atom("_BASP"));
      bro->remove_published_actor(whom, port);
    });
  }
}

} // namespace io
} // namespace caf
