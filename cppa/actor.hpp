#ifndef ACTOR_HPP
#define ACTOR_HPP

#include <memory>
#include <cstdint>
#include <type_traits>

#include "cppa/group.hpp"
#include "cppa/channel.hpp"
#include "cppa/attachable.hpp"
#include "cppa/process_information.hpp"

#include "cppa/util/rm_ref.hpp"
#include "cppa/util/enable_if.hpp"

namespace cppa {

class serializer;
class deserializer;

/**
 * @brief
 */
class actor : public channel
{

    bool m_is_proxy;
    std::uint32_t m_id;
    process_information_ptr m_parent_process;

 protected:

    actor(const process_information_ptr& parent = process_information::get());

    actor(std::uint32_t aid,
          const process_information_ptr& parent = process_information::get());

 public:

    ~actor();

    /**
     * @brief Attaches @p ptr to this actor
     *        (the actor takes ownership of @p ptr).
     *
     * The actor will call <tt>ptr->detach(...)</tt> on exit or immediately
     * if he already exited.
     *
     * @return @c true if @p ptr was successfully attached to the actor;
     *         otherwise (actor already exited) @p false.
     */
    virtual bool attach(attachable* ptr) = 0;

    template<typename F>
    bool attach_functor(F&& ftor);

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
    virtual bool remove_backlink(intrusive_ptr<actor>& to) = 0;

    /**
     * @brief
     * @return
     */
    virtual bool establish_backlink(intrusive_ptr<actor>& to) = 0;

    // rvalue support
    void link_to(intrusive_ptr<actor>&& other);
    void unlink_from(intrusive_ptr<actor>&& other);
    bool remove_backlink(intrusive_ptr<actor>&& to);
    bool establish_backlink(intrusive_ptr<actor>&& to);

    /**
     * @brief Gets the {@link process_information} of the parent process.
     */
    inline const process_information& parent_process() const;

    /**
     * @brief Gets the {@link process_information} pointer
     *        of the parent process.
     */
    inline process_information_ptr parent_process_ptr() const;

    /**
     * @brief
     * @return
     */
    inline std::uint32_t id() const;

    /**
     * @brief
     * @return
     */
    static intrusive_ptr<actor> by_id(std::uint32_t actor_id);

};

typedef intrusive_ptr<actor> actor_ptr;

serializer& operator<<(serializer&, const actor_ptr&);

deserializer& operator>>(deserializer&, actor_ptr&);

/******************************************************************************
 *             inline and template member function implementations            *
 ******************************************************************************/

inline const process_information& actor::parent_process() const
{
    return *m_parent_process;
}

inline process_information_ptr actor::parent_process_ptr() const
{
    return m_parent_process;
}

inline std::uint32_t actor::id() const
{
    return m_id;
}

template<typename T>
bool actor::attach(std::unique_ptr<T>&& ptr,
                typename util::enable_if<std::is_base_of<attachable,T>>::type*)
{
    return attach(static_cast<attachable*>(ptr.release()));
}

template<class F>
class functor_attachable : public attachable
{

    F m_functor;

 public:

    template<class FArg>
    functor_attachable(FArg&& arg) : m_functor(std::forward<FArg>(arg))
    {
    }

    virtual void detach(std::uint32_t reason)
    {
        m_functor(reason);
    }

};

template<typename F>
bool actor::attach_functor(F&& ftor)
{
    typedef typename util::rm_ref<F>::type f_type;
    return attach(new functor_attachable<f_type>(std::forward<F>(ftor)));
}

} // namespace cppa

#endif // ACTOR_HPP
