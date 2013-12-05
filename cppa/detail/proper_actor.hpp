#ifndef PROPER_ACTOR_HPP
#define PROPER_ACTOR_HPP

#include "cppa/mailbox_element.hpp"

namespace cppa { namespace util { class fiber; } }

namespace cppa { namespace detail {

template<class Base,
         class SchedulingPolicy,
         class PriorityPolicy,
         class ResumePolicy,
         class InvokePolicy>
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
        m_resume_policy.launch(this);
    }

    inline mailbox_element* next_message() {
        m_priority_policy.next_message(this);
    }

    void invoke_message(mailbox_element* msg) override {
        m_invoke_policy.invoke(this, msg);
    }

 private:

    SchedulingPolicy m_scheduling_policy;
    PriorityPolicy   m_priority_policy;
    ResumePolicy     m_resume_policy;
    InvokePolicy     m_invoke_policy;

};

} // namespace cppa::detail

#endif // PROPER_ACTOR_HPP
