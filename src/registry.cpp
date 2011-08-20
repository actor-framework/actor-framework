#include "cppa/config.hpp"

#include "cppa/registry.hpp"

namespace {

typedef std::lock_guard<cppa::util::shared_spinlock> exclusive_guard;
typedef cppa::util::shared_lock_guard<cppa::util::shared_spinlock> shared_guard;

void context_cleanup_fun(cppa::context* what)
{
    if (what && !what->deref()) delete what;
}

} // namespace <anonmyous>

namespace cppa {
    
    registry::registry() : m_ids(1), m_contexts(context_cleanup_fun)
    {
    }
    
    registry::~registry()
    {
    }
    
    std::uint32_t registry::next_id()
    {
        return m_ids.fetch_add(1);
    }
    
    void registry::add(actor& actor)
    {
        exclusive_guard guard(m_instances_mtx);
        m_instances.insert(std::make_pair(actor.id(), &actor));
	}
    
    void registry::remove(actor& actor)
    {
        exclusive_guard guard(m_instances_mtx);
        m_instances.erase(m_id);
    }

    intrusive_ptr<actor> registry::by_id(std::uint32_t actor_id)
    {
        shared_guard guard(m_instances_mtx);
        auto i = m_instances.find(actor_id);
        if (i != m_instances.end())
        {
            return i->second;
        }
        return nullptr;
    }

    context* registry::current_context()
    {
        context* result = m_contexts.get();
        if (result == nullptr)
        {
            result = new converted_thread_context(*this);
            result->ref();
            get_scheduler()->register_converted_context(result);
            m_contexts.reset(result);
        }
        return result;
    }

    context* registry::unchecked_current_context()
    {
        return m_contexts.get()
    }

    void registry::set_current_context(context* ctx)
    {
        if (ctx) ctx->ref();
        m_contexts.reset(ctx);
    }

} // namespace cppa
