#include "cppa/exit_signal.hpp"

namespace cppa {

exit_signal::exit_signal() : m_reason(exit_reason::normal)
{
}

exit_signal::exit_signal(std::uint32_t r) : m_reason(r)
{
}

void exit_signal::set_reason(std::uint32_t value)
{
    m_reason = value;
}

} // namespace cppa
