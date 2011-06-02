#include <sstream>
#include "cppa/actor_exited.hpp"

namespace cppa {

actor_exited::~actor_exited() throw()
{
}

actor_exited::actor_exited(std::uint32_t exit_reason) : m_reason(exit_reason)
{
    std::ostringstream oss;
    oss << "actor exited with reason " << exit_reason;
    m_what = oss.str();
}

std::uint32_t actor_exited::reason() const
{
    return m_reason;
}

const char* actor_exited::what() const throw()
{
    return m_what.c_str();
}

} // namespace cppa
