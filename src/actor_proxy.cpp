#include "cppa/atom.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/actor_proxy.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/detail/mailman.hpp"

namespace {

inline constexpr std::uint64_t to_int(cppa::atom_value value)
{
    return static_cast<std::uint64_t>(value);
}

} // namespace <anonymous>

namespace cppa {

actor_proxy::actor_proxy(std::uint32_t mid, const process_information_ptr& pptr)
    : super(mid, pptr)
{
    attach(get_scheduler()->register_hidden_context());
}

void actor_proxy::forward_message(const process_information_ptr& piptr,
                                  const any_tuple& msg)
{
    detail::mailman_queue().push_back(new detail::mailman_job(piptr, msg));
}

void actor_proxy::enqueue(any_tuple&& msg)
{
    any_tuple tmp(std::move(msg));
    enqueue(tmp);
}

void actor_proxy::enqueue(const any_tuple& msg)
{
    if (msg.size() > 0 && msg.utype_info_at(0) == typeid(atom_value))
    {
        if (msg.size() == 2 && msg.utype_info_at(1) == typeid(actor_ptr))
        {
            switch (to_int(msg.get_as<atom_value>(0)))
            {
                case to_int(atom(":Link")):
                {
                    auto s = msg.get_as<actor_ptr>(1);
                    link_to(s);
                    return;
                }
                case to_int(atom(":Unlink")):
                {
                    auto s = msg.get_as<actor_ptr>(1);
                    unlink_from(s);
                    return;
                }
                default: break;
            }
        }
        else if (   msg.size() == 2
                 && msg.get_as<atom_value>(0) == atom(":KillProxy")
                 && msg.utype_info_at(1) == typeid(std::uint32_t))
        {
            cleanup(msg.get_as<std::uint32_t>(1));
            return;
        }
    }
    forward_message(parent_process_ptr(), msg);
}

void actor_proxy::link_to(intrusive_ptr<actor>& other)
{
    if (link_to_impl(other))
    {
        // causes remote actor to link to (proxy of) other
        forward_message(parent_process_ptr(),
                        make_tuple(atom(":Link"), actor_ptr(this)));
    }
}

void actor_proxy::unlink_from(intrusive_ptr<actor>& other)
{
    if (unlink_from_impl(other))
    {
        // causes remote actor to unlink from (proxy of) other
        forward_message(parent_process_ptr(),
                        make_tuple(atom(":Unlink"), actor_ptr(this)));
    }
}

bool actor_proxy::establish_backlink(intrusive_ptr<actor>& other)
{
    bool result = super::establish_backlink(other);
    if (result)
    {
        forward_message(parent_process_ptr(),
                        make_tuple(atom(":Link"), actor_ptr(this)));
    }
    return result;
}

bool actor_proxy::remove_backlink(intrusive_ptr<actor>& other)
{
    bool result = super::remove_backlink(other);
    if (result)
    {
        forward_message(parent_process_ptr(),
                        make_tuple(atom(":Unlink"), actor_ptr(this)));
    }
    return result;
}

} // namespace cppa
