#ifndef ACTOR_HPP
#define ACTOR_HPP

#include <memory>
#include <cstdint>
#include <type_traits>

#include "cppa/group.hpp"
#include "cppa/channel.hpp"
#include "cppa/attachable.hpp"
#include "cppa/process_information.hpp"

#include "cppa/util/enable_if.hpp"

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
     * @brief Detaches the first attached object that matches @p what.
     */
    virtual void detach(const attachable::token& what) = 0;

    template<typename T>
    bool attach(std::unique_ptr<T>&& ptr,
            typename util::enable_if<std::is_base_of<attachable,T>>::type* = 0);

    /**
     * @brief Forces this actor to subscribe to the group @p what.
     *
     * The group will be unsubscribed if the actor exits.
     */
    void join(group_ptr& what);

    /**
     * @brief Forces this actor to leave the group @p what.
     */
    void leave(const group_ptr& what);

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

template<typename T>
bool actor::attach(std::unique_ptr<T>&& ptr,
                typename util::enable_if<std::is_base_of<attachable,T>>::type*)
{
    return attach(static_cast<attachable*>(ptr.release()));
}



} // namespace cppa

#endif // ACTOR_HPP
