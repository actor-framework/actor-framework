#include <boost/thread.hpp>

#include "cppa/detail/yield_interface.hpp"

namespace {

void cleanup_fun(cppa::util::fiber*) { }

boost::thread_specific_ptr<cppa::util::fiber> t_caller(cleanup_fun);
boost::thread_specific_ptr<cppa::util::fiber> t_callee(cleanup_fun);
boost::thread_specific_ptr<cppa::detail::yield_state> t_ystate;

} // namespace <anonymous>

namespace cppa { namespace detail {

void yield(yield_state ystate)
{
    if (t_ystate.get() == nullptr)
    {
        t_ystate.reset(new yield_state(ystate));
    }
    else
    {
        *t_ystate = ystate;
    }
    util::fiber::swap(*t_callee, *t_caller);
}

yield_state yielded_state()
{
    return (t_ystate.get() == nullptr) ? yield_state::invalid : *t_ystate;
}

void call(util::fiber* what, util::fiber* from)
{
    t_ystate.reset();
    t_caller.reset(from);
    t_callee.reset(what);
    util::fiber::swap(*from, *what);
}

} } // namespace cppa::detail
