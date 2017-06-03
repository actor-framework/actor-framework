/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#ifndef CAF_IO_DETAIL_FLARE_ACTOR_HPP
#define CAF_IO_DETAIL_FLARE_ACTOR_HPP

#include <type_traits>

#include "caf/blocking_actor.hpp"

#include "caf/io/detail/flare.hpp"

namespace caf {
namespace io {
namespace detail {

class flare_actor;

} // namespace detail
} // namespace io
} // namespace caf

namespace caf {
namespace mixin {

template <>
struct is_blocking_requester<caf::io::detail::flare_actor> : std::true_type { };

} // namespace mixin
} // namespace caf

namespace caf {
namespace io {
namespace detail {

class flare_actor : public blocking_actor {
public:
  flare_actor(actor_config& sys);

  void launch(execution_unit*, bool, bool) override;

  void act() override;

  void await_data() override;

  bool await_data(timeout_type timeout) override;

  void enqueue(mailbox_element_ptr ptr, execution_unit*) override;

  mailbox_element_ptr dequeue() override;

  const char* name() const override;

  int descriptor() const;

private:
  flare flare_;
  bool await_flare_;
};

} // namespace detail
} // namespace io
} // namespace caf

#endif // CAF_IO_DETAIL_FLARE_ACTOR_HPP
