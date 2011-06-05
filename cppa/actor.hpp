#ifndef ACTOR_HPP
#define ACTOR_HPP

#include <cstdint>

#include "cppa/group.hpp"
#include "cppa/channel.hpp"
#include "cppa/attachable.hpp"
#include "cppa/process_information.hpp"

namespace cppa {

class serializer;
class actor_proxy;
class deserializer;

/**
 * @brief
 */
class actor : public channel
{

    friend class actor_proxy;

    bool m_is_proxy;
    std::uint32_t m_id;

    actor(std::uint32_t aid);

 protected:

    actor();

 public:

    ~actor();

    /**
     * @brief Attaches @p ptr to this actor.
     *
     * @p ptr will be deleted if actor finished execution of immediately
     * if the actor already exited.
     *
     * @return @c true if @p ptr was successfully attached to the actor;
     *         otherwise (actor already exited) @p false.
     *
     */
    virtual bool attach(attachable* ptr) = 0;

    /**
     * @brief
     */
    virtual void join(group_ptr& what) = 0;

    /**
     * @brief
     */
    virtual void leave(const group_ptr& what) = 0;

    /**
     * @brief
     */
    virtual void link_to(intrusive_ptr<actor>& other) = 0;

    /**
     * @brief
     */
    virtual void unlink_from(intrusive_ptr<actor>& other) = 0;

    /**
     * @brief
     * @return
     */
    virtual bool remove_backlink(const intrusive_ptr<actor>& to) = 0;

    /**
     * @brief
     * @return
     */
    virtual bool establish_backlink(const intrusive_ptr<actor>& to) = 0;

    /**
     * @brief Gets the {@link process_information} of the parent process.
     */
    virtual const process_information& parent_process() const;

    /**
     * @brief
     * @return
     */
    inline std::uint32_t id() const { return m_id; }

    /**
     * @brief
     * @return
     */
    static intrusive_ptr<actor> by_id(std::uint32_t actor_id);

};

typedef intrusive_ptr<actor> actor_ptr;

serializer& operator<<(serializer&, const actor_ptr&);

deserializer& operator>>(deserializer&, actor_ptr&);

} // namespace cppa

#endif // ACTOR_HPP
