#ifndef ACTOR_HPP
#define ACTOR_HPP

#include <cstdint>

#include "cppa/group.hpp"
#include "cppa/channel.hpp"

namespace cppa {

class serializer;
class deserializer;

/**
 * @brief
 */
class actor : public channel
{

    std::uint32_t m_id;

 protected:

    actor();

 public:

    ~actor();

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
    virtual void link(intrusive_ptr<actor>& other) = 0;

    /**
     * @brief
     */
    virtual void unlink(intrusive_ptr<actor>& other) = 0;

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
