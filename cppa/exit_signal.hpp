#ifndef EXIT_SIGNAL_HPP
#define EXIT_SIGNAL_HPP

#include <cstdint>

namespace cppa {

enum class exit_reason : std::uint32_t
{

    /**
     * @brief Indicates that an actor finished execution.
     */
    normal = 0x00000,

    /**
     * @brief Indicates that an actor finished execution
     *        because of an unhandled exception.
     */
    unhandled_exception = 0x00001,

    /**
     * @brief Indicates that an actor finishied execution
     *        because a connection to a remote link was
     *        closed unexpectedly.
     */
    remote_link_unreachable = 0x00101,

    /**
     * @brief Any user defined exit reason should have a
     *        value greater or equal to prevent collisions
     *        with default defined exit reasons.
     */
    user_defined = 0x10000

};

static constexpr std::uint32_t user_defined_exit_reason
        = static_cast<std::uint32_t>(exit_reason::user_defined);

/**
 * @brief Converts {@link exit_reason} to @c std::uint32_t.
 */
constexpr std::uint32_t to_uint(exit_reason r)
{
    return static_cast<std::uint32_t>(r);
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
     * @pre {@code r != exit_reason::user_defined}.
     */
    exit_signal(exit_reason r);

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

    // an unambiguous member function
    void set_uint_reason(std::uint32_t value);

    /**
     * @brief Sets the exit reason to @p value.
     */
    void set_reason(std::uint32_t value);

    /**
     * @brief Sets the exit reason to @p value.
     */
    void set_reason(exit_reason value);

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
