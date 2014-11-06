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

#ifndef CAF_DETAIL_PROPER_ACTOR_HPP
#define CAF_DETAIL_PROPER_ACTOR_HPP

#include <type_traits>

#include "caf/fwd.hpp"
#include "caf/duration.hpp"
#include "caf/mailbox_element.hpp"

#include "caf/detail/logging.hpp"

#include "caf/policy/scheduling_policy.hpp"

namespace caf {
namespace detail {

// 'imports' all member functions from policies to the actor,
// the resume mixin also adds the m_hidden member which *must* be
// initialized to `true`
template <class Base, class Derived, class Policies>
class proper_actor_base : public Policies::resume_policy::template
                                 mixin<Base, Derived> {
  using super = typename Policies::resume_policy::template mixin<Base, Derived>;

 public:
  template <class... Ts>
  proper_actor_base(Ts&&... args) : super(std::forward<Ts>(args)...) {
    CAF_REQUIRE(this->is_registered() == false);
  }

  // member functions from scheduling policy

  using timeout_type = typename Policies::scheduling_policy::timeout_type;

  void enqueue(const actor_addr& sender,
         message_id mid,
         message msg,
         execution_unit* eu) override {
    CAF_PUSH_AID(dptr()->id());
    /*
    CAF_LOG_DEBUG(CAF_TARG(sender, to_string)
             << ", " << CAF_MARG(mid, integer_value) << ", "
             << CAF_TARG(msg, to_string));
    */
    scheduling_policy().enqueue(dptr(), sender, mid, msg, eu);
  }

  inline void launch(bool hide, execution_unit* host) {
    CAF_LOG_TRACE("");
    this->is_registered(!hide);
    this->scheduling_policy().launch(this, host);
  }

  template <class F>
  bool fetch_messages(F cb) {
    return scheduling_policy().fetch_messages(dptr(), cb);
  }

  template <class F>
  bool try_fetch_messages(F cb) {
    return scheduling_policy().try_fetch_messages(dptr(), cb);
  }

  template <class F>
  policy::timed_fetch_result fetch_messages(F cb, timeout_type abs_time) {
    return scheduling_policy().fetch_messages(dptr(), cb, abs_time);
  }

  // member functions from priority policy

  inline unique_mailbox_element_pointer next_message() {
    return priority_policy().next_message(dptr());
  }

  inline bool has_next_message() {
    return priority_policy().has_next_message(dptr());
  }

  inline void push_to_cache(unique_mailbox_element_pointer ptr) {
    priority_policy().push_to_cache(std::move(ptr));
  }

  using cache_iterator = typename Policies::priority_policy::cache_iterator;

  inline bool cache_empty() {
    return priority_policy().cache_empty();
  }

  inline cache_iterator cache_begin() {
    return priority_policy().cache_begin();
  }

  inline cache_iterator cache_end() {
    return priority_policy().cache_end();
  }

  inline unique_mailbox_element_pointer cache_take_first() {
    return priority_policy().cache_take_first();
  }

  template <class Iterator>
  inline void cache_prepend(Iterator first, Iterator last) {
    priority_policy().cache_prepend(first, last);
  }

  inline void cache_erase(cache_iterator iter) {
    priority_policy().cache_erase(iter);
  }

  // member functions from resume policy

  // NOTE: resume_policy::resume is implemented in the mixin

  // member functions from invoke policy

  template <class PartialFunctionOrBehavior>
  inline bool invoke_message(unique_mailbox_element_pointer& ptr,
                 PartialFunctionOrBehavior& fun,
                 message_id awaited_response) {
    return invoke_policy().invoke_message(dptr(), ptr, fun,
                        awaited_response);
  }

 protected:
  inline typename Policies::scheduling_policy& scheduling_policy() {
    return m_policies.get_scheduling_policy();
  }

  inline typename Policies::priority_policy& priority_policy() {
    return m_policies.get_priority_policy();
  }

  inline typename Policies::resume_policy& resume_policy() {
    return m_policies.get_resume_policy();
  }

  inline typename Policies::invoke_policy& invoke_policy() {
    return m_policies.get_invoke_policy();
  }

  inline Derived* dptr() {
    return static_cast<Derived*>(this);
  }

 private:
  Policies m_policies;
};

// this is the nonblocking version of proper_actor; it assumes  that Base is
// derived from local_actor and uses the behavior_stack_based mixin
template <class Base, class Policies,
          bool OverrideDequeue = std::is_base_of<blocking_actor, Base>::value>
class proper_actor
    : public proper_actor_base<Base, proper_actor<Base, Policies, false>,
                               Policies> {
  using super = proper_actor_base<Base, proper_actor, Policies>;

 public:
  static_assert(std::is_base_of<local_actor, Base>::value,
                "Base is not derived from local_actor");

  template <class... Ts>
  proper_actor(Ts&&... args) : super(std::forward<Ts>(args)...) {
    // nop
  }

  // required by event_based_resume::mixin::resume

  bool invoke_message(unique_mailbox_element_pointer& ptr) {
    CAF_LOG_TRACE("");
    auto bhvr = this->bhvr_stack().back();
    auto mid = this->bhvr_stack().back_id();
    return this->invoke_policy().invoke_message(this, ptr, bhvr, mid);
  }

  bool invoke_message_from_cache() {
    CAF_LOG_TRACE("");
    auto bhvr = this->bhvr_stack().back();
    auto mid = this->bhvr_stack().back_id();
    auto e = this->cache_end();
    CAF_LOG_DEBUG(std::distance(this->cache_begin(), e)
             << " elements in cache");
    for (auto i = this->cache_begin(); i != e; ++i) {
      auto im = this->invoke_policy().invoke_message(this, *i, bhvr, mid);
      if (im || !*i) {
        this->cache_erase(i);
        if (im) return true;
        // start anew, because we have invalidated our iterators now
        return invoke_message_from_cache();
      }
    }
    return false;
  }
};

// for blocking actors, there's one more member function to implement
template <class Base, class Policies>
class proper_actor<Base, Policies, true>
    : public proper_actor_base<Base, proper_actor<Base, Policies, true>,
                               Policies> {
  using super = proper_actor_base<Base, proper_actor, Policies>;

 public:
  template <class... Ts>
  proper_actor(Ts&&... args)
      : super(std::forward<Ts>(args)...), m_next_timeout_id(0) {}

  // 'import' optional blocking member functions from policies

  inline void await_data() {
    this->scheduling_policy().await_data(this);
  }

  inline void await_ready() {
    this->resume_policy().await_ready(this);
  }

  // implement blocking_actor::dequeue_response

  void dequeue_response(behavior& bhvr, message_id mid) override {
    // try to dequeue from cache first
    if (!this->cache_empty()) {
      std::vector<unique_mailbox_element_pointer> tmp_vec;
      auto restore_cache = [&] {
        if (!tmp_vec.empty()) {
          this->cache_prepend(tmp_vec.begin(), tmp_vec.end());
        }
      };
      while (!this->cache_empty()) {
        auto tmp = this->cache_take_first();
        if (this->invoke_message(tmp, bhvr, mid)) {
          restore_cache();
          return;
        } else
          tmp_vec.push_back(std::move(tmp));
      }
      restore_cache();
    }
    bool has_timeout = false;
    uint32_t timeout_id;
    // request timeout if needed
    if (bhvr.timeout().valid()) {
      has_timeout = true;
      timeout_id = this->request_timeout(bhvr.timeout());
    }
    // workaround for GCC 4.7 bug (const this when capturing refs)
    auto& pending_timeouts = m_pending_timeouts;
    auto guard = detail::make_scope_guard([&] {
      if (has_timeout) {
        auto e = pending_timeouts.end();
        auto i = std::find(pending_timeouts.begin(), e, timeout_id);
        if (i != e) {
          pending_timeouts.erase(i);
        }
      }
    });
    // read incoming messages
    for (;;) {
      auto msg = this->next_message();
      if (!msg) {
        this->await_ready();
      } else {
        if (this->invoke_message(msg, bhvr, mid)) {
          // we're done
          return;
        }
        if (msg) this->push_to_cache(std::move(msg));
      }
    }
  }

  // timeout handling

  uint32_t request_timeout(const duration& d) {
    CAF_REQUIRE(d.valid());
    auto tid = ++m_next_timeout_id;
    auto msg = make_message(timeout_msg{tid});
    if (d.is_zero()) {
      // immediately enqueue timeout message if duration == 0s
      this->enqueue(this->address(), invalid_message_id,
                    std::move(msg), this->host());
      // auto e = this->new_mailbox_element(this, std::move(msg));
      // this->m_mailbox.enqueue(e);
    } else {
      this->delayed_send_tuple(this, d, std::move(msg));
    }
    m_pending_timeouts.push_back(tid);
    return tid;
  }

  inline void handle_timeout(behavior& bhvr, uint32_t timeout_id) {
    auto e = m_pending_timeouts.end();
    auto i = std::find(m_pending_timeouts.begin(), e, timeout_id);
    CAF_LOG_WARNING_IF(i == e, "ignored unexpected timeout");
    if (i != e) {
      m_pending_timeouts.erase(i);
      bhvr.handle_timeout();
    }
  }

  // required by nestable invoke policy
  inline void pop_timeout() {
    m_pending_timeouts.pop_back();
  }

  // required by nestable invoke policy;
  // adds a dummy timeout to the pending timeouts to prevent
  // nestable invokes to trigger an inactive timeout
  inline void push_timeout() {
    m_pending_timeouts.push_back(++m_next_timeout_id);
  }

  inline bool waits_for_timeout(uint32_t timeout_id) const {
    auto e = m_pending_timeouts.end();
    auto i = std::find(m_pending_timeouts.begin(), e, timeout_id);
    return i != e;
  }

  inline bool is_active_timeout(uint32_t tid) const {
    return !m_pending_timeouts.empty() && m_pending_timeouts.back() == tid;
  }

 private:
  std::vector<uint32_t> m_pending_timeouts;
  uint32_t m_next_timeout_id;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_PROPER_ACTOR_HPP
