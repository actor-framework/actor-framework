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

#ifndef CAF_IO_BROKER_HPP
#define CAF_IO_BROKER_HPP

#include <map>
#include <vector>

#include "caf/spawn.hpp"
#include "caf/extend.hpp"
#include "caf/local_actor.hpp"

#include "caf/mixin/functor_based.hpp"

#include "caf/io/abstract_broker.hpp"

namespace caf {
namespace io {

/**
 * A broker mediates between actor systems and other components in the network.
 * @extends local_actor
 */
class broker : public abstract_broker,
               public abstract_event_based_actor<behavior, false> {
 public:
  using super = abstract_event_based_actor<behavior, false>;

  class continuation;

  /** @cond PRIVATE */

  void initialize() override;

  template <class F, class... Ts>
  actor fork(F fun, connection_handle hdl, Ts&&... xs) {
    // provoke compile-time errors early
    using fun_res = decltype(fun(this, hdl, std::forward<Ts>(xs)...));
    // prevent warning about unused local type
    static_assert(std::is_same<fun_res, fun_res>::value,
                  "your compiler is lying to you");
    auto i = m_scribes.find(hdl);
    if (i == m_scribes.end()) {
      CAF_LOG_ERROR("invalid handle");
      throw std::invalid_argument("invalid handle");
    }
    auto sptr = i->second;
    CAF_ASSERT(sptr->hdl() == hdl);
    m_scribes.erase(i);
    return spawn_functor(nullptr, [sptr](broker* forked) {
                                    sptr->set_broker(forked);
                                    forked->m_scribes.emplace(sptr->hdl(),
                                                              sptr);
                                  },
                         fun, hdl, std::forward<Ts>(xs)...);
  }

  bool is_initialized() override {
    return super::is_initialized();
  }

  actor_addr address() const override {
    return super::address();
  }

  void invoke_message(mailbox_element_ptr& msg) override;

  void enqueue(const actor_addr&, message_id,
               message, execution_unit*) override;

  void enqueue(mailbox_element_ptr, execution_unit*) override;

  class functor_based;

  void launch(execution_unit* eu, bool lazy, bool hide);

  void cleanup(uint32_t reason);

 protected:
  broker() {}

  broker(middleman& parent_ref) : abstract_broker(parent_ref) {}

  virtual behavior make_behavior() = 0;

  /**
   * Can be overridden to perform cleanup code before the
   * broker closes all its connections.
   */
  void on_exit() override;

 private:
  bool invoke_message_from_cache() override;
};

class broker::functor_based : public extend<broker>::
                                     with<mixin::functor_based> {
 public:
  using super = combined_type;

  template <class... Ts>
  functor_based(Ts&&... xs) : super(std::forward<Ts>(xs)...) {
    // nop
  }

  ~functor_based();

  behavior make_behavior() override;
};

using broker_ptr = intrusive_ptr<broker>;

} // namespace io
} // namespace caf

#endif // CAF_IO_BROKER_HPP
