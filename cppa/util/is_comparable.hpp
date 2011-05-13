#ifndef IS_COMPARABLE_HPP
#define IS_COMPARABLE_HPP

#include <type_traits>

namespace cppa { namespace util {

template<typename T1, typename T2>
class is_comparable
{

    // SFINAE: If you pass a "bool*" as third argument, then
    //         decltype(cmp_help_fun(...)) is bool if there's an
    //         operator==(A,B) because
    //         cmp_help_fun(A*, B*, bool*) is a better match than
    //         cmp_help_fun(A*, B*, void*). If there's no operator==(A,B)
    //         available, then cmp_help_fun(A*, B*, void*) is the only
    //         candidate and thus decltype(cmp_help_fun(...)) is void.

    template<typename A, typename B>
    static bool cmp_help_fun(const A* arg0, const B* arg1,
                             decltype(*arg0 == *arg1)* = nullptr)
    {
        return true;
    }

    template<typename A, typename B>
    static void cmp_help_fun(const A*, const B*, void* = nullptr) { }

    typedef decltype(cmp_help_fun(static_cast<T1*>(nullptr),
                                  static_cast<T2*>(nullptr),
                                  static_cast<bool*>(nullptr)))
            result_type;

 public:

    static const bool value = std::is_same<bool, result_type>::value;

};

} } // namespace cppa::util

#endif // IS_COMPARABLE_HPP
