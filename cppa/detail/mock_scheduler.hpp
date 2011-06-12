#ifndef MOCK_SCHEDULER_HPP
#define MOCK_SCHEDULER_HPP

#include "cppa/scheduler.hpp"

namespace cppa { namespace detail {

class mock_scheduler : public scheduler
{

 public:

    void await_others_done();
    void register_converted_context(context*);
    //void unregister_converted_context(context*);
    actor_ptr spawn(actor_behavior*, scheduling_hint);
    std::unique_ptr<attachable> register_hidden_context();

};

} } // namespace cppa::detail

#endif // MOCK_SCHEDULER_HPP
