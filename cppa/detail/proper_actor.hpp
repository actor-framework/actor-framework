#ifndef PROPER_ACTOR_HPP
#define PROPER_ACTOR_HPP

#include <type_traits>

#include "cppa/logging.hpp"
#include "cppa/blocking_actor.hpp"
#include "cppa/mailbox_element.hpp"

#include "cppa/policy/scheduling_policy.hpp"

#include "cppa/util/duration.hpp"

namespace cppa {
namespace detail {

// 'imports' all member functions from policies to the actor,
// the resume mixin also adds the m_hidden member which *must* be
// initialized to @p true
template<class Base,
         class Derived,
         class Policies>
class proper_actor_base : public Policies::resume_policy::template mixin<Base, Derived> {

    typedef typename Policies::resume_policy::template mixin<Base, Derived> super;

 public:

    template <typename... Ts>
    proper_actor_base(Ts&&... args) : super(std::forward<Ts>(args)...) {
        CPPA_REQUIRE(this->m_hidden == true);
    }

    // grant access to the actor's mailbox
    typename Base::mailbox_type& mailbox() {
        return this->m_mailbox;
    }

    // member functions from scheduling policy

    typedef typename Policies::scheduling_policy::timeout_type timeout_type;

    void enqueue(msg_hdr_cref hdr, any_tuple msg, execution_unit* eu) override {
        CPPA_PUSH_AID(dptr()->id());
        CPPA_LOG_DEBUG(CPPA_TARG(hdr, to_string)
                       << ", " << CPPA_TARG(msg, to_string));
        scheduling_policy().enqueue(dptr(), hdr, msg, eu);
    }

    // NOTE: scheduling_policy::launch is 'imported' in proper_actor

    template<typename F>
    bool fetch_messages(F cb) {
        return scheduling_policy().fetch_messages(dptr(), cb);
    }

    template<typename F>
    bool try_fetch_messages(F cb) {
        return scheduling_policy().try_fetch_messages(dptr(), cb);
    }

    template<typename F>
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

    typedef typename Policies::priority_policy::cache_iterator cache_iterator;

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

    template<typename Iterator>
    inline void cache_prepend(Iterator first, Iterator last) {
        priority_policy().cache_prepend(first, last);
    }

    inline void cache_erase(cache_iterator iter) {
        priority_policy().cache_erase(iter);
    }

    // member functions from resume policy

    // NOTE: resume_policy::resume is implemented in the mixin

    // member functions from invoke policy

    template<class PartialFunctionOrBehavior>
    inline bool invoke_message(unique_mailbox_element_pointer& ptr,
                               PartialFunctionOrBehavior& fun,
                               message_id awaited_response) {
        return invoke_policy().invoke_message(dptr(), ptr, fun,
                                              awaited_response);
    }

    inline bool hidden() const {
        return this->m_hidden;
    }

    void cleanup(std::uint32_t reason) override {
        CPPA_LOG_TRACE(CPPA_ARG(reason));
        if (!hidden()) get_actor_registry()->dec_running();
        super::cleanup(reason);
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

    inline void hidden(bool value) {
        if (this->m_hidden == value) return;
        if (value) get_actor_registry()->dec_running();
        else get_actor_registry()->inc_running();
        this->m_hidden = value;
    }

 private:

    Policies m_policies;

};

template<class Base,
         class Policies,
         bool OverrideDequeue = std::is_base_of<blocking_actor, Base>::value>
class proper_actor : public proper_actor_base<Base,
                                              proper_actor<Base,
                                                           Policies,
                                                           false>,
                                              Policies> {

    typedef proper_actor_base<Base, proper_actor, Policies> super;

 public:

    static_assert(std::is_base_of<local_actor, Base>::value,
                  "Base is not derived from local_actor");

    // this is the nonblocking version of proper_actor; it assumes
    // that Base is derived from local_actor and uses the
    // behavior_stack_based mixin

    template <typename... Ts>
    proper_actor(Ts&&... args) : super(std::forward<Ts>(args)...) { }

    inline void launch(bool is_hidden, execution_unit* host) {
        CPPA_LOG_TRACE("");
        this->hidden(is_hidden);
        auto bhvr = this->make_behavior();
        if (bhvr) this->become(std::move(bhvr));
        CPPA_LOG_WARNING_IF(this->bhvr_stack().empty(),
                            "actor did not set a behavior");
        if (!this->bhvr_stack().empty()) {
            this->scheduling_policy().launch(this, host);
        }
    }

    // required by event_based_resume::mixin::resume

    bool invoke_message(unique_mailbox_element_pointer& ptr) {
        CPPA_LOG_TRACE("");
        auto bhvr = this->bhvr_stack().back();
        auto mid = this->bhvr_stack().back_id();
        return this->invoke_policy().invoke_message(this, ptr, bhvr, mid);
    }

    bool invoke_message_from_cache() {
        CPPA_LOG_TRACE("");
        auto bhvr = this->bhvr_stack().back();
        auto mid = this->bhvr_stack().back_id();
        auto e = this->cache_end();
        CPPA_LOG_DEBUG(std::distance(this->cache_begin(), e)
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
class proper_actor<Base, Policies,true> : public proper_actor_base<Base,
                                              proper_actor<Base,
                                                           Policies,
                                                           true>,
                                              Policies> {

    typedef proper_actor_base<Base, proper_actor, Policies> super;

 public:

    template <typename... Ts>
    proper_actor(Ts&&... args)
        : super(std::forward<Ts>(args)...), m_next_timeout_id(0) { }

    // 'import' optional blocking member functions from policies

    inline void await_data() {
        this->scheduling_policy().await_data(this);
    }

    inline void await_ready() {
        this->resume_policy().await_ready(this);
    }

    inline void launch(bool is_hidden, execution_unit* host) {
        this->hidden(is_hidden);
        this->scheduling_policy().launch(this, host);
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
                }
                else tmp_vec.push_back(std::move(tmp));
            }
            restore_cache();
        }
        bool has_timeout = false;
        std::uint32_t timeout_id;
        // request timeout if needed
        if (bhvr.timeout().valid()) {
            has_timeout = true;
            timeout_id = this->request_timeout(bhvr.timeout());
        }
        // workaround for GCC 4.7 bug (const this when capturing refs)
        auto& pending_timeouts = m_pending_timeouts;
        auto guard = util::make_scope_guard([&] {
            if (has_timeout) {
                auto e = pending_timeouts.end();
                auto i = std::find(pending_timeouts.begin(), e, timeout_id);
                if (i != e) pending_timeouts.erase(i);
            }
        });
        // read incoming messages
        for (;;) {
            auto msg = this->next_message();
            if (!msg) this->await_ready();
            else {
                if (this->invoke_message(msg, bhvr, mid)) {
                    // we're done
                    return;
                }
                if (msg) this->push_to_cache(std::move(msg));
            }
        }
    }

    // timeout handling

    std::uint32_t request_timeout(const util::duration& d) {
        CPPA_REQUIRE(d.valid());
        auto tid = ++m_next_timeout_id;
        auto msg = make_any_tuple(timeout_msg{tid});
        if (d.is_zero()) {
            // immediately enqueue timeout message if duration == 0s
            this->enqueue({this->address(), this},
                          std::move(msg), this->m_host);
            //auto e = this->new_mailbox_element(this, std::move(msg));
            //this->m_mailbox.enqueue(e);
        }
        else this->delayed_send_tuple(this, d, std::move(msg));
        m_pending_timeouts.push_back(tid);
        return tid;
    }

    inline void handle_timeout(behavior& bhvr, std::uint32_t timeout_id) {
        auto e = m_pending_timeouts.end();
        auto i = std::find(m_pending_timeouts.begin(), e, timeout_id);
        CPPA_LOG_WARNING_IF(i == e, "ignored unexpected timeout");
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

    inline bool waits_for_timeout(std::uint32_t timeout_id) const {
        auto e = m_pending_timeouts.end();
        auto i = std::find(m_pending_timeouts.begin(), e, timeout_id);
        return i != e;
    }

    inline bool is_active_timeout(std::uint32_t tid) const {
        return !m_pending_timeouts.empty() && m_pending_timeouts.back() == tid;
    }

 private:

    std::vector<std::uint32_t> m_pending_timeouts;
    std::uint32_t m_next_timeout_id;

};

} // namespace detail
} // namespace cppa

#endif // PROPER_ACTOR_HPP
