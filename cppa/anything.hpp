#ifndef LIBCPPA_ANYTHING_HPP
#define LIBCPPA_ANYTHING_HPP

namespace cppa {

/**
 * @brief Acts as wildcard pattern.
 */
struct anything { };

inline bool operator==(anything const&, anything const&)
{
    return true;
}

inline bool operator!=(anything const&, anything const&)
{
    return false;
}

} // namespace cppa

#endif // ANYTHING_HPP
