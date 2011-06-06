#ifndef ACTOR_IMPL_UTIL_HPP
#define ACTOR_IMPL_UTIL_HPP

#include <atomic>
#include <memory>

#include "cppa/attachable.hpp"
#include "cppa/exit_reason.hpp"

namespace cppa { namespace detail {

template<class Guard, class List, class Mutex>
bool do_attach(std::atomic<std::uint32_t>& reason,
               unique_attachable_ptr&& uptr,
               List& ptr_list,
               Mutex& mtx)
{
    if (uptr == nullptr)
    {
        Guard guard(mtx);
        return reason.load() == exit_reason::not_exited;
    }
    else
    {
        std::uint32_t reason_value;
        // lifetime scope of guard
        {
            Guard guard(mtx);
            reason_value = reason.load();
            if (reason_value == exit_reason::not_exited)
            {
                ptr_list.push_back(std::move(uptr));
                return true;
            }
        }
        uptr->detach(reason_value);
        return false;
    }
}

template<class Guard, class List, class Mutex>
void do_detach(const attachable::token& what, List& ptr_list, Mutex& mtx)
{
    Guard guard(mtx);
    for (auto i = ptr_list.begin(); i != ptr_list.end(); ++i)
    {
        if ((*i)->matches(what))
        {
            ptr_list.erase(i);
            return;
        }
    }
}

} } // namespace cppa::detail

#endif // ACTOR_IMPL_UTIL_HPP
