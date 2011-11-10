#ifndef LIBCPPA_ANYTHING_HPP
#define LIBCPPA_ANYTHING_HPP

namespace cppa {

/**
 * @brief Acts as wildcard pattern.
 */
struct anything { };

inline bool operator==(const anything&, const anything&)
{
    return true;
}

inline bool operator!=(const anything&, const anything&)
{
    return false;
}

} // namespace cppa

#endif // ANYTHING_HPP
