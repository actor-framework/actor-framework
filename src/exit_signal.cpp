#include "cppa/exit_signal.hpp"

namespace cppa {

exit_signal::exit_signal() : m_reason(to_uint(exit_reason::normal))
{
}

exit_signal::exit_signal(exit_reason r) : m_reason(to_uint(r))
{
}

exit_signal::exit_signal(std::uint32_t r) : m_reason(r)
{
}

void exit_signal::set_reason(std::uint32_t value)
{
    m_reason = value;
}

void exit_signal::set_reason(exit_reason value)
{
    m_reason = to_uint(value);
}

void exit_signal::set_uint_reason(std::uint32_t value)
{
    set_reason(value);
}

} // namespace cppa
