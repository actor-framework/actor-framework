#ifndef LIBCPPA_UTIL_VOID_TYPE_HPP
#define LIBCPPA_UTIL_VOID_TYPE_HPP

namespace cppa { namespace util {

// forward declaration
template<typename... Types> struct type_list;

struct void_type
{
    typedef void_type head_type;
    typedef type_list<> tail_type;
};

inline bool operator==(const void_type&, const void_type&) { return true; }

} } // namespace cppa::util

#endif // LIBCPPA_UTIL_VOID_TYPE_HPP
