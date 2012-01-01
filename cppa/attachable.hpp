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

    attachable(attachable const&) = delete;
    attachable& operator=(attachable const&) = delete;

 protected:

    attachable() = default;

 public:

    struct token
    {
        std::type_info const& subtype;
        void const* ptr;
        inline token(std::type_info const& msubtype, void const* mptr)
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

    virtual bool matches(token const&) = 0;

};

} // namespace cppa

#endif // ATTACHABLE_HPP
