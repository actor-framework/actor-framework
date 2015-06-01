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

#include "caf/none.hpp"
#include "caf/config.hpp"
#include "caf/make_counted.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/singletons.hpp"
#include "caf/detail/scope_guard.hpp"

#include "caf/io/broker.hpp"
#include "caf/io/middleman.hpp"

#include "caf/detail/actor_registry.hpp"
#include "caf/detail/sync_request_bouncer.hpp"

namespace caf {
namespace io {

class broker::continuation {
 public:
  continuation(broker_ptr bptr, mailbox_element_ptr mptr)
      : m_self(std::move(bptr)),
        m_ptr(std::move(mptr)) {
    // nop
  }

  continuation(continuation&&) = default;

  inline void operator()() {
    CAF_PUSH_AID(m_self->id());
    CAF_LOG_TRACE("");
    m_self->invoke_message(m_ptr);
  }

 private:
  broker_ptr m_self;
  mailbox_element_ptr m_ptr;
};

void broker::invoke_message(mailbox_element_ptr& ptr) {
  CAF_LOG_TRACE(CAF_TARG(ptr->msg, to_string));
  if (exit_reason() != exit_reason::not_exited || !has_behavior()) {
    CAF_LOG_DEBUG("actor already finished execution"
                  << ", planned_exit_reason = " << planned_exit_reason()
                  << ", has_behavior() = " << has_behavior());
    if (ptr->mid.valid()) {
      detail::sync_request_bouncer srb{exit_reason()};
      srb(ptr->sender, ptr->mid);
    }
    return;
  }
  // prepare actor for invocation of message handler
  try {
    auto& bhvr = this->awaits_response()
                 ? this->awaited_response_handler()
                 : this->bhvr_stack().back();
    auto bid = this->awaited_response_id();
    switch (local_actor::invoke_message(ptr, bhvr, bid)) {
      case im_success: {
        CAF_LOG_DEBUG("handle_message returned hm_msg_handled");
        while (has_behavior()
               && planned_exit_reason() == exit_reason::not_exited
               && invoke_message_from_cache()) {
          // rinse and repeat
        }
        break;
      }
      case im_dropped:
        CAF_LOG_DEBUG("handle_message returned hm_drop_msg");
        break;
      case im_skipped: {
        CAF_LOG_DEBUG("handle_message returned hm_skip_msg or hm_cache_msg");
        if (ptr) {
          m_cache.push_second_back(ptr.release());
        }
        break;
      }
    }
  }
  catch (std::exception& e) {
    CAF_LOG_INFO("broker killed due to an unhandled exception: "
                 << to_verbose_string(e));
    // keep compiler happy in non-debug mode
    static_cast<void>(e);
    quit(exit_reason::unhandled_exception);
  }
  catch (...) {
    CAF_LOG_ERROR("broker killed due to an unknown exception");
    quit(exit_reason::unhandled_exception);
  }
  // safe to actually release behaviors now
  bhvr_stack().cleanup();
  // cleanup actor if needed
  if (planned_exit_reason() != exit_reason::not_exited) {
    cleanup(planned_exit_reason());
  } else if (!has_behavior()) {
    CAF_LOG_DEBUG("no behavior set, quit for normal exit reason");
    quit(exit_reason::normal);
    cleanup(planned_exit_reason());
  }
}

bool broker::invoke_message_from_cache() {
  CAF_LOG_TRACE("");
  auto& bhvr = this->awaits_response()
               ? this->awaited_response_handler()
               : this->bhvr_stack().back();
  auto bid = this->awaited_response_id();
  auto i = m_cache.second_begin();
  auto e = m_cache.second_end();
  CAF_LOG_DEBUG(std::distance(i, e) << " elements in cache");
  return m_cache.invoke(static_cast<local_actor*>(this), i, e, bhvr, bid);
}

void broker::enqueue(mailbox_element_ptr ptr, execution_unit*) {
  backend().post(continuation{this, std::move(ptr)});
}

void broker::enqueue(const actor_addr& sender, message_id mid, message msg,
                     execution_unit* eu) {
  enqueue(mailbox_element::make(sender, mid, std::move(msg)), eu);
}

void broker::cleanup(uint32_t reason) {
  CAF_LOG_TRACE(CAF_ARG(reason));
  planned_exit_reason(reason);
  on_exit();
  close_all();
  CAF_ASSERT(m_doormen.empty());
  CAF_ASSERT(m_scribes.empty());
  CAF_ASSERT(current_mailbox_element() == nullptr);
  m_cache.clear();
  super::cleanup(reason);
  deref(); // release implicit reference count from middleman
}

void broker::on_exit() {
  // nop
}

void broker::launch(execution_unit*, bool, bool is_hidden) {
  // add implicit reference count held by the middleman
  ref();
  is_registered(!is_hidden);
  CAF_PUSH_AID(id());
  CAF_LOGF_TRACE("init and launch broker with ID " << id());
  // we want to make sure initialization is executed in MM context
  become(
    [=](sys_atom) {
      CAF_LOGF_TRACE("ID " << id());
      unbecome();
      // launch backends now, because user-defined initialization
      // might call functions like add_connection
      for (auto& kvp : m_doormen) {
        kvp.second->launch();
      }
      super::is_initialized(true);
      // run user-defined initialization code
      auto bhvr = make_behavior();
      if (bhvr) {
        become(std::move(bhvr));
      }
    }
  );
  enqueue(invalid_actor_addr, invalid_message_id,
          make_message(sys_atom::value), nullptr);
}

void broker::initialize() {
  // nop
}

broker::functor_based::~functor_based() {
  // nop
}

behavior broker::functor_based::make_behavior() {
  return m_make_behavior(this);
}

} // namespace io
} // namespace caf
