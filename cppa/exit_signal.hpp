#ifndef EXIT_SIGNAL_HPP
#define EXIT_SIGNAL_HPP

#include <cstdint>

namespace cppa {

namespace exit_reason
{

    /**
     * @brief Indicates that the actor is still alive.
     */
    static constexpr std::uint32_t not_exited = 0x00000;

    /**
     * @brief Indicates that an actor finished execution.
     */
    static constexpr std::uint32_t normal = 0x00001;

    /**
     * @brief Indicates that an actor finished execution
     *        because of an unhandled exception.
     */
    static constexpr std::uint32_t unhandled_exception = 0x00002;

    /**
     * @brief Indicates that an actor finishied execution
     *        because a connection to a remote link was
     *        closed unexpectedly.
     */
    static constexpr std::uint32_t remote_link_unreachable = 0x00101;

    /**
     * @brief Any user defined exit reason should have a
     *        value greater or equal to prevent collisions
     *        with default defined exit reasons.
     */
    static constexpr std::uint32_t user_defined = 0x10000;

}

class exit_signal
{

    std::uint32_t m_reason;

 public:

    /**
     * @brief Creates an exit signal with
     *        <tt>reason() == exit_reason::normal</tt>.
     */
    exit_signal();

    /**
     * @brief Creates an exit signal with
     *        <tt>reason() == @p r</tt>.
     * @pre {@code r >= exit_reason::user_defined}.
     */
    explicit exit_signal(std::uint32_t r);

    /**
     * @brief Reads the exit reason.
     */
    inline std::uint32_t reason() const
    {
        return m_reason;
    }

    /**
     * @brief Sets the exit reason to @p value.
     */
    void set_reason(std::uint32_t value);

};

inline bool operator==(const exit_signal& lhs, const exit_signal& rhs)
{
    return lhs.reason() == rhs.reason();
}

inline bool operator!=(const exit_signal& lhs, const exit_signal& rhs)
{
    return !(lhs == rhs);
}

} // namespace cppa

#endif // EXIT_SIGNAL_HPP
