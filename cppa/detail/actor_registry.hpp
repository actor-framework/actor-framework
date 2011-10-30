#ifndef ACTOR_REGISTRY_HPP
#define ACTOR_REGISTRY_HPP

#include <map>
#include <atomic>
#include <cstdint>

#include "cppa/actor.hpp"
#include "cppa/detail/thread.hpp"
#include "cppa/util/shared_spinlock.hpp"

namespace cppa { namespace detail {

class actor_registry
{

 public:

    actor_registry();

    // adds @p whom to the list of known actors
    void add(actor* whom);

    // removes @p whom from the list of known actors
    void remove(actor* whom);

    // looks for an actor with id @p whom in the list of known actors
    actor_ptr find(actor_id whom);

    // gets the next free actor id
    actor_id next_id();

    // increases running-actors-count by one
    void inc_running();

    // decreases running-actors-count by one
    void dec_running();

    size_t running();

    // blocks the caller until running-actors-count becomes @p expected
    void await_running_count_equal(size_t expected);

 private:

    std::atomic<size_t> m_running;
    std::atomic<std::uint32_t> m_ids;

    mutex m_running_mtx;
    condition_variable m_running_cv;

    util::shared_spinlock m_instances_mtx;
    std::map<std::uint32_t, actor*> m_instances;

};

} } // namespace cppa::detail

#endif // ACTOR_REGISTRY_HPP
