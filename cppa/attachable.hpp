#ifndef ATTACHABLE_HPP
#define ATTACHABLE_HPP

#include <cstdint>
#include <typeinfo>

namespace cppa {

// utilty class;
// provides a virtual destructor that's executed after an actor
// finished execution
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
    virtual void detach(std::uint32_t reason);

    virtual bool matches(const token&);

};

} // namespace cppa

#endif // ATTACHABLE_HPP
