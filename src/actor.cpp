#include "cppa/actor.hpp"

namespace cppa {

actor::actor() : m_id(0)
{
}

intrusive_ptr<actor> actor::by_id(std::uint32_t)
{
    return nullptr;
}

} // namespace cppa
