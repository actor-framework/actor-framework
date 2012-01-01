#ifndef HAS_COPY_MEMBER_FUN_HPP
#define HAS_COPY_MEMBER_FUN_HPP

#include <type_traits>

namespace cppa { namespace util {

template<typename T>
class has_copy_member_fun
{

    template<typename A>
    static bool hc_help_fun(A const* arg0, decltype(arg0->copy()) = 0)
    {
        return true;
    }

    template<typename A>
    static void hc_help_fun(A const*, void* = 0) { }

    typedef decltype(hc_help_fun((T*) 0, (T*) 0)) result_type;

 public:

    static const bool value = std::is_same<bool, result_type>::value;

};

} } // namespace cppa::util

#endif // HAS_COPY_MEMBER_FUN_HPP
