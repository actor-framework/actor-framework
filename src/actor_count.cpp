#include "cppa/detail/actor_count.hpp"
#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/singleton_manager.hpp"

namespace cppa { namespace detail {

void inc_actor_count()
{
    singleton_manager::get_actor_registry()->inc_running();
}

void dec_actor_count()
{
    singleton_manager::get_actor_registry()->dec_running();
}

void actor_count_wait_until(size_t value)
{
    singleton_manager::get_actor_registry()->await_running_count_equal(value);
}

} } // namespace cppa::detail
