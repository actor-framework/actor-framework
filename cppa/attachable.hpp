#ifndef ATTACHABLE_HPP
#define ATTACHABLE_HPP

#include <cstdint>
#include <typeinfo>

namespace cppa {

/**
 * @brief Callback utility class.
 */
class attachable
{

    attachable(const attachable&) = delete;
    attachable& operator=(const attachable&) = delete;

 protected:

    attachable() = default;

 public:

    struct token
    {
        const std::type_info& subtype;
        const void* ptr;
        inline token(const std::type_info& msubtype, const void* mptr)
            : subtype(msubtype), ptr(mptr)
        {
        }
    };

    virtual ~attachable();

    /**
     * @brief Executed if the actor finished execution
     *        with given @p reason.
     *
     * The default implementation does nothing.
     */
    virtual void actor_exited(std::uint32_t reason) = 0;

    virtual bool matches(const token&) = 0;

};

} // namespace cppa

#endif // ATTACHABLE_HPP
