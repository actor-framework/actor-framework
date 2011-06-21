#include "cppa/detail/delegate.hpp"

namespace cppa { namespace detail {

void delegate::operator()()
{
    m_fun(m_arg1, m_arg2);
}

} } // namespace cppa::detail
