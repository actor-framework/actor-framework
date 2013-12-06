#ifndef NO_RESUME_HPP
#define NO_RESUME_HPP

#include <chrono>

#include "cppa/policy/resume_policy.hpp"

namespace cppa { namespace policy {

class no_resume {

 public:

    using timeout_type = std::chrono::high_resolution_clock::time_point;

    template<class Actor>
    timeout_type init_timeout(Actor* self, const util::duration& rel_time) {
        // request explicit timeout message
        self->request_timeout(rel_time);
        return 0; // return some dummy value
    }

    template<class Actor, typename F>
    void fetch_messages(Actor* self, F cb) {
        auto e = self->m_mailbox.try_pop();
        while (e) {
            cb(e);
            e = self->m_mailbox.try_pop();
        }
    }

    template<class Actor, typename F>
    inline void fetch_messages(Actor* self, F cb, timeout_type) {
        // a call to this call is always preceded by init_timeout,
        // which will trigger a timeout message
        fetch_messages(self, cb);
    }

    template<class Actor, typename F>
    void try_fetch_messages(Actor* self, F cb) {
        // try_fetch and fetch have the same semantics for event-based actors
        fetch_messages(self, cb);
    }

    template<class Actor>
    resume_result resume(Actor* self, util::fiber*) {
        CPPA_LOG_TRACE("id = " << self->id()
                       << ", state = " << static_cast<int>(self->state()));
        CPPA_REQUIRE(   self->state() == actor_state::ready
                     || self->state() == actor_state::pending);
        auto done_cb = [&]() -> bool {
            CPPA_LOG_TRACE("");
            if (self->exit_reason() == self->exit_reason::not_exited) {
                if (self->planned_exit_reason() == self->exit_reason::not_exited) {
                    self->planned_exit_reason(exit_reason::normal);
                }
                self->on_exit();
                if (!m_bhvr_stack.empty()) {
                    planned_exit_reason(exit_reason::not_exited);
                    return false; // on_exit did set a new behavior
                }
                cleanup(planned_exit_reason());
            }
            set_state(actor_state::done);
            m_bhvr_stack.clear();
            m_bhvr_stack.cleanup();
            on_exit();
            CPPA_REQUIRE(next_job == nullptr);
            return true;
        };
        CPPA_REQUIRE(next_job == nullptr);
        try {
            //auto e = m_mailbox.try_pop();
            for (auto e = m_mailbox.try_pop(); ; e = m_mailbox.try_pop()) {
                //e = m_mailbox.try_pop();
                if (e == nullptr) {
                    CPPA_REQUIRE(next_job == nullptr);
                    CPPA_LOGMF(CPPA_DEBUG, self, "no more element in mailbox; going to block");
                    set_state(actor_state::about_to_block);
                    std::atomic_thread_fence(std::memory_order_seq_cst);
                    if (this->m_mailbox.can_fetch_more() == false) {
                        switch (compare_exchange_state(actor_state::about_to_block,
                                                       actor_state::blocked)) {
                            case actor_state::ready:
                                // interrupted by arriving message
                                // restore members
                                CPPA_REQUIRE(m_chained_actor == nullptr);
                                CPPA_LOGMF(CPPA_DEBUG, self, "switched back to ready: "
                                               "interrupted by arriving message");
                                break;
                            case actor_state::blocked:
                                CPPA_LOGMF(CPPA_DEBUG, self, "set state successfully to blocked");
                                // done setting actor to blocked
                                return resume_result::actor_blocked;
                            default:
                                CPPA_LOGMF(CPPA_ERROR, self, "invalid state");
                                CPPA_CRITICAL("invalid state");
                        };
                    }
                    else {
                        CPPA_LOGMF(CPPA_DEBUG, self, "switched back to ready: "
                                       "mailbox can fetch more");
                        set_state(actor_state::ready);
                        CPPA_REQUIRE(m_chained_actor == nullptr);
                    }
                }
                else {
                    CPPA_LOGMF(CPPA_DEBUG, self, "try to invoke message: " << to_string(e->msg));
                    if (m_bhvr_stack.invoke(m_recv_policy, this, e)) {
                        CPPA_LOG_DEBUG_IF(m_chained_actor,
                                          "set actor with ID "
                                          << m_chained_actor->id()
                                          << " as successor");
                        if (m_bhvr_stack.empty() && done_cb()) {
                            CPPA_LOGMF(CPPA_DEBUG, self, "behavior stack empty");
                            return resume_result::actor_done;
                        }
                        m_bhvr_stack.cleanup();
                    }
                }
            }
        }
        catch (actor_exited& what) {
            CPPA_LOG_INFO("actor died because of exception: actor_exited, "
                          "reason = " << what.reason());
            if (exit_reason() == exit_reason::not_exited) {
                quit(what.reason());
            }
        }
        catch (std::exception& e) {
            CPPA_LOG_WARNING("actor died because of exception: "
                             << detail::demangle(typeid(e))
                             << ", what() = " << e.what());
            if (exit_reason() == exit_reason::not_exited) {
                quit(exit_reason::unhandled_exception);
            }
        }
        catch (...) {
            CPPA_LOG_WARNING("actor died because of an unknown exception");
            if (exit_reason() == exit_reason::not_exited) {
                quit(exit_reason::unhandled_exception);
            }
        }
        done_cb();
        return resume_result::actor_done;
    }

};

} } // namespace cppa::policy

#endif // NO_RESUME_HPP
