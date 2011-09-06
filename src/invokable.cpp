#include "cppa/detail/invokable.hpp"

namespace cppa { namespace detail {

invokable_base::~invokable_base()
{
}

timed_invokable::timed_invokable(const util::duration& d) : m_timeout(d)
{
}

} } // namespace cppa::detail
