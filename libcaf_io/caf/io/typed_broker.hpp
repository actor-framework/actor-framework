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

#ifndef CAF_IO_TYPED_BROKER_HPP
#define CAF_IO_TYPED_BROKER_HPP

#include <map>
#include <vector>

#include "caf/none.hpp"
#include "caf/config.hpp"
#include "caf/make_counted.hpp"
#include "caf/spawn.hpp"
#include "caf/extend.hpp"
#include "caf/typed_behavior.hpp"
#include "caf/local_actor.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/singletons.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/detail/actor_registry.hpp"
#include "caf/detail/sync_request_bouncer.hpp"

#include "caf/mixin/typed_functor_based.hpp"

#include "caf/io/middleman.hpp"
#include "caf/io/abstract_broker.hpp"

namespace caf {
namespace io {

/**
 * A typed broker mediates between actor systems and other components in
 * the network.
 * @extends local_actor
 */
template <class... Sigs>
class typed_broker : public abstract_broker,
                     public abstract_event_based_actor<
                       typed_behavior<Sigs...>, false
                     > {
 public:
  using signatures = detail::type_list<Sigs...>;

  using behavior_type = typed_behavior<Sigs...>;

  using super = abstract_event_based_actor<behavior_type, false>;

  using pointer = intrusive_ptr<typed_broker>;

  // friend with all possible instantiations
  template <class...>
  friend class typed_broker;

  class continuation {
   public:
    continuation(pointer bptr, mailbox_element_ptr mptr)
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
    pointer m_self;
    mailbox_element_ptr m_ptr;
  };

  using super::send;
  using super::delayed_send;

  template <class... DestSigs, class... Ts>
  void send(message_priority mp, const typed_actor<Sigs...>& dest, Ts&&... xs) {
    detail::sender_signature_checker<
      detail::type_list<Sigs...>,
      detail::type_list<DestSigs...>,
      detail::type_list<
        typename detail::implicit_conversions<
          typename std::decay<Ts>::type
        >::type...
      >
    >::check();
    super::send(mp, dest, std::forward<Ts>(xs)...);
  }

  template <class... DestSigs, class... Ts>
  void send(const typed_actor<DestSigs...>& dest, Ts&&... xs) {
    detail::sender_signature_checker<
      detail::type_list<Sigs...>,
      detail::type_list<DestSigs...>,
      detail::type_list<
        typename detail::implicit_conversions<
          typename std::decay<Ts>::type
        >::type...
      >
    >::check();
    super::send(dest, std::forward<Ts>(xs)...);
  }

  template <class... DestSigs, class... Ts>
  void delayed_send(message_priority mp, const typed_actor<Sigs...>& dest,
                    const duration& rtime, Ts&&... xs) {
    detail::sender_signature_checker<
      detail::type_list<Sigs...>,
      detail::type_list<DestSigs...>,
      detail::type_list<
        typename detail::implicit_conversions<
          typename std::decay<Ts>::type
        >::type...
      >
    >::check();
    super::delayed_send(mp, dest, rtime, std::forward<Ts>(xs)...);
  }

  template <class... DestSigs, class... Ts>
  void delayed_send(const typed_actor<Sigs...>& dest,
                    const duration& rtime, Ts&&... xs) {
    detail::sender_signature_checker<
      detail::type_list<Sigs...>,
      detail::type_list<DestSigs...>,
      detail::type_list<
        typename detail::implicit_conversions<
          typename std::decay<Ts>::type
        >::type...
      >
    >::check();
    super::delayed_send(dest, rtime, std::forward<Ts>(xs)...);
  }

  /** @cond PRIVATE */
  std::set<std::string> message_types() const override {
    return {Sigs::static_type_name()...};
  }

  void initialize() override {
    // nop
  }

  template <class F, class... Ts>
  typename infer_typed_actor_handle<
    typename detail::get_callable_trait<F>::result_type,
    typename detail::tl_head<
      typename detail::get_callable_trait<F>::arg_types
    >::type
  >::type
  fork(F fun, connection_handle hdl, Ts&&... xs) {
    auto i = m_scribes.find(hdl);
    if (i == m_scribes.end()) {
      CAF_LOG_ERROR("invalid handle");
      throw std::invalid_argument("invalid handle");
    }
    auto sptr = i->second;
    CAF_ASSERT(sptr->hdl() == hdl);
    m_scribes.erase(i);
    using trait = typename detail::get_callable_trait<F>::type;
    using arg_types = typename trait::arg_types;
    using first_arg = typename detail::tl_head<arg_types>::type;
    using base_class = typename std::remove_pointer<first_arg>::type;
    using impl_class = typename base_class::functor_based;
    return spawn_class<impl_class>(nullptr,
                                   [sptr](impl_class* forked) {
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

  void invoke_message(mailbox_element_ptr& ptr) override {
    CAF_LOG_TRACE(CAF_TARG(ptr->msg, to_string));
    if (super::exit_reason() != exit_reason::not_exited
        || !super::has_behavior()) {
      CAF_LOG_DEBUG("actor already finished execution"
                    << ", planned_exit_reason = "
                    << super::planned_exit_reason()
                    << ", has_behavior() = " << super::has_behavior());
      if (ptr->mid.valid()) {
        detail::sync_request_bouncer srb{super::exit_reason()};
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
          while (super::has_behavior()
                 && super::planned_exit_reason() == exit_reason::not_exited
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
      super::quit(exit_reason::unhandled_exception);
    }
    catch (...) {
      CAF_LOG_ERROR("broker killed due to an unknown exception");
      super::quit(exit_reason::unhandled_exception);
    }
    // safe to actually release behaviors now
    super::bhvr_stack().cleanup();
    // cleanup actor if needed
    if (super::planned_exit_reason() != exit_reason::not_exited) {
      cleanup(super::planned_exit_reason());
    } else if (!super::has_behavior()) {
      CAF_LOG_DEBUG("no behavior set, quit for normal exit reason");
      super::quit(exit_reason::normal);
      cleanup(super::planned_exit_reason());
    }
  }

  void enqueue(const actor_addr& sender, message_id mid,
               message msg, execution_unit* eu) override {
    enqueue(mailbox_element::make(sender, mid, std::move(msg)), eu);
  }

  void enqueue(mailbox_element_ptr ptr, execution_unit*) override {
    backend().post(continuation{this, std::move(ptr)});
  }

  class functor_based : public extend<typed_broker>::
                               template with<mixin::typed_functor_based> {
   public:
    using super = typename extend<typed_broker>::
                  template with<mixin::typed_functor_based>;

    template <class... Ts>
    functor_based(Ts&&... xs) : super(std::forward<Ts>(xs)...) {
      // nop
    }

    ~functor_based() {
      // nop
    }

    behavior_type make_behavior() override {
      return super::m_make_behavior(this);
    }
  };

  void launch(execution_unit*, bool, bool is_hidden) {
    // add implicit reference count held by the middleman
    super::ref();
    super::is_registered(!is_hidden);
    CAF_PUSH_AID(super::id());
    CAF_LOGF_TRACE("init and launch broker with ID " << id());
    // we want to make sure initialization is executed in MM context
    super::do_become(
      [=](sys_atom) {
        CAF_LOGF_TRACE("ID " << id());
        super::unbecome();
        // launch backends now, because user-defined initialization
        // might call functions like add_connection
        for (auto& kvp : m_doormen) {
          kvp.second->launch();
        }
        super::is_initialized(true);
        // run user-defined initialization code
        auto bhvr = make_behavior();
        if (bhvr) {
          super::become(std::move(bhvr));
        }
      },
      true
    );
    enqueue(invalid_actor_addr, invalid_message_id,
            make_message(sys_atom::value), nullptr);
  }

  void cleanup(uint32_t reason) {
    CAF_LOG_TRACE(CAF_ARG(reason));
    super::planned_exit_reason(reason);
    on_exit();
    close_all();
    CAF_ASSERT(m_doormen.empty());
    CAF_ASSERT(m_scribes.empty());
    CAF_ASSERT(super::current_mailbox_element() == nullptr);
    m_cache.clear();
    super::cleanup(reason);
    super::deref(); // release implicit reference count from middleman
  }

 protected:
  typed_broker() {}

  typed_broker(middleman& parent_ref) : abstract_broker(parent_ref) {}

  virtual behavior_type make_behavior() = 0;

  /**
   * Can be overridden to perform cleanup code before the
   * broker closes all its connections.
   */
  void on_exit() override {
    // nop
  }

 private:
  bool invoke_message_from_cache() override {
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
};

} // namespace io
} // namespace caf

#endif // CAF_IO_TYPED_BROKER_HPP
