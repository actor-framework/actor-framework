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

#include "caf/config.hpp"

#include "caf/actor_marker.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/io/network/default_multiplexer.hpp"
#include "caf/io/network/event_handler.hpp"
#include "caf/io/network/native_socket.hpp"
#include "caf/mixin/behavior_changer.hpp"
#include "caf/mixin/requester.hpp"
#include "caf/mixin/sender.hpp"

namespace caf {
namespace io {
namespace network {

// -- forward declarations -----------------------------------------------------

struct newb_base;

} // namespace network
} // namespace io

// -- required to make newb an actor ------------------------------------------

template <>
class behavior_type_of<io::network::newb_base> {
public:
  using type = behavior;
};

namespace io {
namespace network {

// -- newb base ----------------------------------------------------------------

struct newb_base : public extend<scheduled_actor, newb_base>::template
                          with<mixin::sender, mixin::requester,
                               mixin::behavior_changer>,
                   public dynamically_typed_actor_base,
                   public network::event_handler {

  using signatures = none_t;

  using super = typename extend<scheduled_actor, newb_base>::
    template with<mixin::sender, mixin::requester, mixin::behavior_changer>;

  // -- constructors and destructors -------------------------------------------

  newb_base(actor_config& cfg, network::default_multiplexer& dm,
            network::native_socket sockfd);

  // -- overridden modifiers of abstract_actor ---------------------------------

  void enqueue(mailbox_element_ptr ptr, execution_unit*) override;

  void enqueue(strong_actor_ptr src, message_id mid, message msg,
               execution_unit*) override;

  resumable::subtype_t subtype() const override;

  const char* name() const override;

  // -- overridden modifiers of local_actor ------------------------------------

  void launch(execution_unit* eu, bool lazy, bool hide) override;

  void initialize() override;

  bool cleanup(error&& reason, execution_unit* host) override;

  // -- overridden modifiers of resumable --------------------------------------

  network::multiplexer::runnable::resume_result resume(execution_unit* ctx,
                                                       size_t mt) override;

  // -- requirements -----------------------------------------------------------

  virtual void start() = 0;

  virtual void stop() = 0;

  virtual void io_error(network::operation op, error err) = 0;

  virtual void start_reading() = 0;

  virtual void stop_reading() = 0;

  virtual void start_writing() = 0;

  virtual void stop_writing() = 0;

  // -- members ----------------------------------------------------------------

  void init_newb();

  /// Override this to set the behavior of the broker.
  virtual behavior make_behavior();
};

} // namespace network
} // namespace io
} // namespace caf
