#ifndef ACTOR_PROXY_HPP
#define ACTOR_PROXY_HPP

#include "cppa/actor.hpp"
#include "cppa/abstract_actor.hpp"

namespace cppa {

/**
 * @brief Represents a remote Actor.
 */
class actor_proxy : public abstract_actor<actor>
{

    typedef abstract_actor<actor> super;

    void forward_message(process_information_ptr const&,
                         actor*, any_tuple const&);

 public:

    actor_proxy(std::uint32_t mid, process_information_ptr const& parent);

    void enqueue(actor* sender, any_tuple&& msg);

    void enqueue(actor* sender, any_tuple const& msg);

    void link_to(intrusive_ptr<actor>& other);

    void unlink_from(intrusive_ptr<actor>& other);

    bool remove_backlink(intrusive_ptr<actor>& to);

    bool establish_backlink(intrusive_ptr<actor>& to);

};

typedef intrusive_ptr<actor_proxy> actor_proxy_ptr;

} // namespace cppa

#endif // ACTOR_PROXY_HPP
