#include "cppa/config.hpp"

#include <assert.h>
#include <map>
#include <stdexcept>

#include "cppa/actor.hpp"
#include "cppa/registry.hpp"

namespace cppa {

actor::actor(std::uint32_t aid, const process_information_ptr& pptr)
    : m_registry(nullptr), m_id(aid), m_parent_process(pptr)
{
#ifdef DEBUG
    m_sig = ACTOR_SIG_ALIVE;
#endif
    if (!pptr)
    {
        throw std::logic_error("parent == nullptr");
    }
    // Do not register proxy actor.
}

actor::actor(registry& registry, const process_information_ptr& pptr)
    : m_registry(&registry), m_id(registry.next_id()), m_parent_process(pptr)
{
#ifdef DEBUG
    m_sig = ACTOR_SIG_ALIVE;
#endif
    if (!pptr)
    {
        throw std::logic_error("parent == nullptr");
    }
	// Register actor.
	m_registry->add(*this);
}

actor::~actor()
{
#ifdef DEBUG
    switch(m_sig) {
	case ACTOR_SIG_ALIVE:
	  m_sig = ACTOR_SIG_DEAD;
	  break;
	case ACTOR_SIG_DEAD:
	  assert("Actor is already destroyed" && false);
	  break;
	default:
	  assert("Invalid actor" && false);
	  break;
	}
#endif
    if (m_registry)
    {
		// Unregister actor.
		m_registry->remove(*this);
    }
}

void actor::join(group_ptr& what)
{
    if (!what) return;
    attach(what->subscribe(this));
}

void actor::leave(const group_ptr& what)
{
    if (!what) return;
    attachable::token group_token(typeid(group::unsubscriber), what.get());
    detach(group_token);
}

void actor::link_to(intrusive_ptr<actor>&& other)
{
    intrusive_ptr<actor> tmp(std::move(other));
    link_to(tmp);
}

void actor::unlink_from(intrusive_ptr<actor>&& other)
{
    intrusive_ptr<actor> tmp(std::move(other));
    unlink_from(tmp);
}

bool actor::remove_backlink(intrusive_ptr<actor>&& to)
{
    intrusive_ptr<actor> tmp(std::move(to));
    return remove_backlink(tmp);
}

bool actor::establish_backlink(intrusive_ptr<actor>&& to)
{
    intrusive_ptr<actor> tmp(std::move(to));
    return establish_backlink(tmp);
}

} // namespace cppa
