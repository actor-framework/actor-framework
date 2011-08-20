#ifndef REGISTRY_HPP
#define REGISTRY_HPP

#include <cstdint>
#include <atomic>
#include <map>
#include <mutex>

// for thread_specific_ptr
// needed unless the new keyword "thread_local" works in GCC
#include <boost/thread.hpp>

#include "cppa/actor.hpp"
#include "cppa/context.hpp"
#include "cppa/util/shared_spinlock.hpp"
#include "cppa/util/shared_lock_guard.hpp"

namespace cppa {

/**
 * @brief Represents a single CPPA instance.
 */
class registry {
	
public:

    registry();
    ~registry();
    
    std::uint32_t next_id();
    
    void add(actor& actor);
    void remove(actor& actor);

    /**
     * @brief Get the actor that has the unique identifier @p actor_id.
     * @return A pointer to the requested actor or @c nullptr if no
     *         running actor with the ID @p actor_id was found.
     */
    static intrusive_ptr<actor> by_id(std::uint32_t actor_id);
    
    /**
     * @brief Returns a pointer to the current active context.
     */
    context* current_context();

    /**
     * @brief Returns active context without creating it if needed.
     */
    context* unchecked_current_context();

    /**
     * @brief Modifies current active context.
     */
    void set_current_context(context*);

private:
	
	std::atomic<std::uint32_t> m_ids;
    std::map<std::uint32_t, actor*> m_instances;
    cppa::util::shared_spinlock m_instances_mtx;

    boost::thread_specific_ptr<cppa::context> m_contexts;
};

} // namespace cppa

#endif // REGISTRY_HPP
