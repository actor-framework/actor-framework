#ifndef IS_ARRAY_OF_HPP
#define IS_ARRAY_OF_HPP

#include <type_traits>

namespace cppa { namespace util {

/**
 * @brief <tt>is_array_of<T,U>::value == true</tt> if and only
 *        if T is an array of U.
 */
template<typename T, typename U>
struct is_array_of
{
    typedef typename std::remove_all_extents<T>::type step1_type;
    typedef typename std::remove_cv<step1_type>::type step2_type;
    static const bool value =    std::is_array<T>::value
                              && std::is_same<step2_type, U>::value;
};

} } // namespace cppa::util

#endif // IS_ARRAY_OF_HPP
