#ifndef PROPER_ACTOR_HPP
#define PROPER_ACTOR_HPP

#include <type_traits>

#include "cppa/blocking_actor.hpp"
#include "cppa/mailbox_element.hpp"

namespace cppa { namespace util { class fiber; } }

namespace cppa { namespace detail {

template<class Base,
         class SchedulingPolicy,
         class PriorityPolicy,
         class ResumePolicy,
         class InvokePolicy,
         bool OverrideDequeue = std::is_base_of<blocking_actor, Base>::value>
class proper_actor : public Base {

 public:

    template<typename... Ts>
    proper_actor(Ts&&... args) : Base(std::forward<Ts>(args)) { }

    void enqueue(const message_header& hdr, any_tuple msg) override {
        m_scheduling_policy.enqueue(this, hdr, msg);
    }

    void resume(util::fiber* fb) override {
        m_resume_policy.resume(this, fb);
    }

    inline void launch() {
        m_scheduling_policy.launch(this);
    }

    inline mailbox_element* next_message() {
        return m_priority_policy.next_message(this);
    }

    inline void invoke(mailbox_element* msg) {
        m_invoke_policy.invoke(this, msg);
    }

    // grant access to the actor's mailbox
    Base::mailbox_type& mailbox() {
        return this->m_mailbox;
    }

 protected:

    SchedulingPolicy m_scheduling_policy;
    PriorityPolicy   m_priority_policy;
    ResumePolicy     m_resume_policy;
    InvokePolicy     m_invoke_policy;

};

// for blocking actors, there's one more member function to implement
template<class Base,
         class SchedulingPolicy,
         class PriorityPolicy,
         class ResumePolicy,
         class InvokePolicy>
class proper_actor<Base,
                   SchedulingPolicy,
                   PriorityPolicy,
                   ResumePolicy,
                   InvokePolicy,
                   true> : public proper_actor<Base,
                                               SchedulingPolicy,
                                               PriorityPolicy,
                                               ResumePolicy,
                                               InvokePolicy,
                                               false> {

 public:

    void dequeue(behavior& bhvr) override {
        if (bhvr.timeout().valid()) {
            auto abs_time = m_scheduling_policy.init_timeout(this, bhvr.timeout());
            auto done = false;
            while (!done) {
                if (!m_resume_policy.await_data(this, abs_time)) {
                    bhvr.handle_timeout();
                    done = true;
                }
                else {
                    auto msg = m_priority_policy.next_message(this);
                    // must not return nullptr, because await_data guarantees
                    // at least one message in our mailbox
                    CPPA_REQUIRE(msg != nullptr);
                    done = m_invoke_policy.invoke(this, bhvr, msg);
                }
            }
        }
        else {
            for (;;) {
                auto msg = m_priority_policy.next_message(this);
                while (msg) {
                    if (m_invoke_policy.invoke(this, bhvr, msg)) {
                        // we're done
                        return;
                    }
                    msg = m_priority_policy.next_message(this);
                }
                m_resume_policy.await_data(this);
            }
        }
    }

};

} } // namespace cppa::detail

#endif // PROPER_ACTOR_HPP
