#ifndef PROPER_ACTOR_HPP
#define PROPER_ACTOR_HPP

#include <type_traits>

#include "cppa/logging.hpp"
#include "cppa/resumable.hpp"
#include "cppa/mailbox_element.hpp"
#include "cppa/blocking_untyped_actor.hpp"

#include "cppa/policy/scheduling_policy.hpp"

#include "cppa/util/duration.hpp"

namespace cppa {
namespace util {
struct fiber;
} // namespace util
} // namespace cppa

namespace cppa {
namespace detail {

// 'imports' all member functions from policies to the actor
template<class Base,
         class Derived,
         class Policies>
class proper_actor_base : public Policies::resume_policy::template mixin<Base, Derived> {

    typedef typename Policies::resume_policy::template mixin<Base, Derived> super;

 public:

    template <typename... Ts>
    proper_actor_base(Ts&&... args)
        : super(std::forward<Ts>(args)...), m_has_pending_tout(false)
        , m_pending_tout(0) { }

    // grant access to the actor's mailbox
    typename Base::mailbox_type& mailbox() {
        return this->m_mailbox;
    }

    mailbox_element* dummy_node() {
        return &this->m_dummy_node;
    }

    // member functions from scheduling policy

    typedef typename Policies::scheduling_policy::timeout_type timeout_type;

    void enqueue(const message_header& hdr, any_tuple msg) override {
        CPPA_PUSH_AID(dptr()->id());
        CPPA_LOG_DEBUG(CPPA_TARG(hdr, to_string)
                       << ", " << CPPA_TARG(msg, to_string));
        scheduling_policy().enqueue(dptr(), hdr, msg);
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

    inline cache_iterator cache_begin() {
        return priority_policy().cache_begin();
    }

    inline cache_iterator cache_end(){
        return priority_policy().cache_end();
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

    // timeout handling

    inline void reset_timeout() {
        if (m_has_pending_tout) {
            ++m_pending_tout;
            m_has_pending_tout = false;
        }
    }

    void request_timeout(const util::duration& d) {
        if (!d.valid()) m_has_pending_tout = false;
        else {
            auto msg = make_any_tuple(atom("SYNC_TOUT"), ++m_pending_tout);
            if (d.is_zero()) {
                // immediately enqueue timeout message if duration == 0s
                dptr()->enqueue({this->address(), this}, std::move(msg));
                //auto e = this->new_mailbox_element(this, std::move(msg));
                //this->m_mailbox.enqueue(e);
            }
            else dptr()->delayed_send_tuple(this, d, std::move(msg));
            m_has_pending_tout = true;
        }
    }

    inline void handle_timeout(behavior& bhvr) {
        bhvr.handle_timeout();
        reset_timeout();
    }

    inline void pop_timeout() {
        CPPA_REQUIRE(m_pending_tout > 0);
        --m_pending_tout;
    }

    inline void push_timeout() {
        ++m_pending_tout;
    }

    inline bool waits_for_timeout(std::uint32_t timeout_id) const {
        return m_has_pending_tout && m_pending_tout == timeout_id;
    }

    inline bool has_pending_timeout() const {
        return m_has_pending_tout;
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
    bool m_has_pending_tout;
    std::uint32_t m_pending_tout;

};

template<class Base,
         class Policies,
         bool OverrideDequeue = std::is_base_of<blocking_untyped_actor, Base>::value>
class proper_actor : public proper_actor_base<Base,
                                              proper_actor<Base,
                                                           Policies,
                                                           false>,
                                              Policies> {

    typedef proper_actor_base<Base, proper_actor, Policies> super;

 public:

    template <typename... Ts>
    proper_actor(Ts&&... args) : super(std::forward<Ts>(args)...) { }

    inline void launch() {
        auto bhvr = this->make_behavior();
        if (bhvr) this->bhvr_stack().push_back(std::move(bhvr));
        this->scheduling_policy().launch(this);
    }

    // implement pure virtual functions from behavior_stack_based

    void become_waiting_for(behavior bhvr, message_id mf) override {
        if (bhvr.timeout().valid()) {
            if (bhvr.timeout().valid()) {
                this->reset_timeout();
                this->request_timeout(bhvr.timeout());
            }
            this->bhvr_stack().push_back(std::move(bhvr), mf);
        }
        this->bhvr_stack().push_back(std::move(bhvr), mf);
    }

    void do_become(behavior bhvr, bool discard_old) override {
        //if (discard_old) m_bhvr_stack.pop_async_back();
        //m_bhvr_stack.push_back(std::move(bhvr));
        this->reset_timeout();
        if (bhvr.timeout().valid()) {
            this->request_timeout(bhvr.timeout());
        }
        if (discard_old) this->m_bhvr_stack.pop_async_back();
        this->m_bhvr_stack.push_back(std::move(bhvr));
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
    proper_actor(Ts&&... args) : super(std::forward<Ts>(args)...) { }

    // 'import' optional blocking member functions from policies

    inline void await_data() {
        this->scheduling_policy().await_data(this);
    }

    inline void await_ready() {
        this->resume_policy().await_ready(this);
    }

    inline void launch() {
        this->scheduling_policy().launch(this);
    }

    // implement blocking_untyped_actor::dequeue_response

    void dequeue_response(behavior& bhvr, message_id mid) override {
        { // try to dequeue from cache first
            auto i = this->cache_begin();
            auto e = this->cache_end();
            for (; i != e; ++i) {
                if (this->invoke_message(*i, bhvr, mid)) {
                    this->cache_erase(i);
                    return;
                }
            }
        }
        // request timeout if needed
        if (bhvr.timeout().valid()) {
            this->request_timeout(bhvr.timeout());
        }
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

};

} // namespace detail
} // namespace cppa

#endif // PROPER_ACTOR_HPP
