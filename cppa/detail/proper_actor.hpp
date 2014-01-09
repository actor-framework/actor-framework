#ifndef PROPER_ACTOR_HPP
#define PROPER_ACTOR_HPP

#include <type_traits>

#include "cppa/logging.hpp"
#include "cppa/resumable.hpp"
#include "cppa/mailbox_element.hpp"
#include "cppa/blocking_untyped_actor.hpp"

#include "cppa/util/duration.hpp"

namespace cppa {
namespace util {
struct fiber;
} // namespace util
} // namespace cppa

namespace cppa {
namespace detail {

template<class Base,
         class SchedulingPolicy,
         class PriorityPolicy,
         class ResumePolicy,
         class InvokePolicy>
class proper_actor_base : public ResumePolicy::template mixin<Base> {

    friend SchedulingPolicy;
    friend PriorityPolicy;
    friend ResumePolicy;
    friend InvokePolicy;

    typedef typename ResumePolicy::template mixin<Base> super;

 public:

    template <typename... Ts>
    proper_actor_base(Ts&&... args) : super(std::forward<Ts>(args)...) { }

    void enqueue(const message_header& hdr, any_tuple msg) override {
        CPPA_PUSH_AID(this->id());
        CPPA_LOG_DEBUG(CPPA_TARG(hdr, to_string)
                       << ", " << CPPA_TARG(msg, to_string));
        m_scheduling_policy.enqueue(this, hdr, msg);
    }

    inline mailbox_element* next_message() {
        return m_priority_policy.next_message(this);
    }

    // grant access to the actor's mailbox
    typename Base::mailbox_type& mailbox() { return this->m_mailbox; }

    SchedulingPolicy& scheduling_policy() { return m_scheduling_policy; }

    PriorityPolicy& priority_policy() { return m_priority_policy; }

    ResumePolicy& resume_policy() { return m_resume_policy; }

    InvokePolicy& invoke_policy() { return m_invoke_policy; }

    inline void push_timeout() {
        m_scheduling_policy.push_timeout();
    }

    inline void request_timeout(const util::duration& rel_timeout) {
        m_invoke_policy.request_timeout(this, rel_timeout);
    }

    detail::behavior_stack& bhvr_stack() { return this->m_bhvr_stack; }

 protected:

    SchedulingPolicy m_scheduling_policy;
    PriorityPolicy m_priority_policy;
    ResumePolicy m_resume_policy;
    InvokePolicy m_invoke_policy;

};

template<class Base,
         class SchedulingPolicy,
         class PriorityPolicy,
         class ResumePolicy,
         class InvokePolicy,
         bool OverrideDequeue = std::is_base_of<blocking_untyped_actor, Base>::value>
class proper_actor : public proper_actor_base<Base, SchedulingPolicy,
                                              PriorityPolicy, ResumePolicy,
                                              InvokePolicy> {

    typedef proper_actor_base<Base, SchedulingPolicy, PriorityPolicy,
                              ResumePolicy, InvokePolicy>
            super;

 public:

    template <typename... Ts>
    proper_actor(Ts&&... args) : super(std::forward<Ts>(args)...) { }

    inline void launch() {
        auto bhvr = this->make_behavior();
        if (bhvr) this->bhvr_stack().push_back(std::move(bhvr));
        this->m_scheduling_policy.launch(this);
    }

    bool invoke(mailbox_element* msg) {
        return this->m_invoke_policy.invoke(this, msg, this->bhvr_stack().back(),
                                            this->bhvr_stack().back_id());
    }

    void become_waiting_for(behavior bhvr, message_id mf) override {
        if (bhvr.timeout().valid()) {
            if (bhvr.timeout().valid()) {
                this->invoke_policy().reset_timeout();
                this->invoke_policy().request_timeout(this, bhvr.timeout());
            }
            this->bhvr_stack().push_back(std::move(bhvr), mf);
        }
        this->bhvr_stack().push_back(std::move(bhvr), mf);
    }

    void do_become(behavior&& bhvr, bool discard_old) override {
        //if (discard_old) m_bhvr_stack.pop_async_back();
        //m_bhvr_stack.push_back(std::move(bhvr));
        this->invoke_policy().reset_timeout();
        if (bhvr.timeout().valid()) {
            this->invoke_policy().request_timeout(this, bhvr.timeout());
        }
        if (discard_old) this->m_bhvr_stack.pop_async_back();
        this->m_bhvr_stack.push_back(std::move(bhvr));
    }

};

// for blocking actors, there's one more member function to implement
template <class Base, class SchedulingPolicy, class PriorityPolicy,
          class ResumePolicy, class InvokePolicy>
class proper_actor<
    Base, SchedulingPolicy, PriorityPolicy, ResumePolicy, InvokePolicy,
    true> final : public proper_actor_base<Base, SchedulingPolicy,
                                           PriorityPolicy, ResumePolicy,
                                           InvokePolicy> {

    typedef proper_actor_base<Base, SchedulingPolicy, PriorityPolicy,
                              ResumePolicy, InvokePolicy>
            super;

 public:

    template <typename... Ts>
    proper_actor(Ts&&... args) : super(std::forward<Ts>(args)...) { }

    inline void launch() {
        this->m_scheduling_policy.launch(this);
    }

    void dequeue_response(behavior& bhvr, message_id mid) override {
        if (bhvr.timeout().valid()) {
            auto tout =
                this->m_scheduling_policy.init_timeout(this, bhvr.timeout());
            auto done = false;
            while (!done) {
                if (!this->m_resume_policy.await_data(this, tout)) {
                    bhvr.handle_timeout();
                    done = true;
                } else {
                    auto msg = this->m_priority_policy.next_message(this);
                    // must not return nullptr, because await_data guarantees
                    // at least one message in our mailbox
                    CPPA_REQUIRE(msg != nullptr);
                    done = this->m_invoke_policy.invoke(this, msg, bhvr, mid);
                }
            }
        } else {
            for (;;) {
                auto msg = this->m_priority_policy.next_message(this);
                while (msg) {
                    if (this->m_invoke_policy.invoke(this, msg, bhvr, mid)) {
                        // we're done
                        return;
                    }
                    msg = this->m_priority_policy.next_message(this);
                }
                this->m_resume_policy.await_data(this);
            }
        }
    }

    mailbox_element* dequeue() override {
        auto e = try_dequeue();
        if (!e) {
            this->m_resume_policy.await_data(this);
            return try_dequeue(); // guaranteed to succeed after await_data
        }
        return e;
    }

    mailbox_element* try_dequeue() override {
        return this->m_priority_policy.next_message(this);
    }

    mailbox_element* try_dequeue(const typename Base::timeout_type& tout) override {
        if (this->m_resume_policy.await_data(this, tout)) {
            return try_dequeue();
        }
        return nullptr;
    }

};

} // namespace detail
} // namespace cppa

#endif // PROPER_ACTOR_HPP
