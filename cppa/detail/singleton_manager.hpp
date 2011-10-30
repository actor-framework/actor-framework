#ifndef SINGLETON_MANAGER_HPP
#define SINGLETON_MANAGER_HPP

namespace cppa
{

class scheduler;
class msg_content;

} // namespace cppa

namespace cppa { namespace detail {

class empty_tuple;
class group_manager;
class abstract_tuple;
class actor_registry;
class network_manager;
class uniform_type_info_map;

class singleton_manager
{

    singleton_manager() = delete;

 public:

    static scheduler* get_scheduler();

    static bool set_scheduler(scheduler*);

    static group_manager* get_group_manager();

    static actor_registry* get_actor_registry();

    // created on-the-fly on a successfull cast to set_scheduler()
    static network_manager* get_network_manager();

    static uniform_type_info_map* get_uniform_type_info_map();

    static abstract_tuple* get_tuple_dummy();

    static empty_tuple* get_empty_tuple();

};

} } // namespace cppa::detail

#endif // SINGLETON_MANAGER_HPP
