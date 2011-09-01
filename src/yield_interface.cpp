#include <memory>

#include "cppa/detail/yield_interface.hpp"

namespace {

using namespace cppa;

__thread util::fiber* t_caller = nullptr;
__thread util::fiber* t_callee = nullptr;
__thread detail::yield_state t_ystate = detail::yield_state::invalid;

} // namespace <anonymous>

namespace cppa { namespace detail {

void yield(yield_state ystate)
{
    t_ystate = ystate;
    util::fiber::swap(*t_callee, *t_caller);
}

yield_state yielded_state()
{
    return t_ystate;
}

void call(util::fiber* what, util::fiber* from)
{
    t_ystate = yield_state::invalid;
    t_caller = from;
    //t_caller.reset(from);
    t_callee = what;
    util::fiber::swap(*from, *what);
}

} } // namespace cppa::detail
