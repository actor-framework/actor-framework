#ifndef IS_COPYABLE_HPP
#define IS_COPYABLE_HPP

#include <type_traits>

namespace cppa { namespace detail {

template<bool IsAbstract, typename T>
class is_copyable_
{

    template<typename A>
    static bool cpy_help_fun(const A* arg0, decltype(new A(*arg0)) = nullptr)
    {
        return true;
    }

    template<typename A>
    static void cpy_help_fun(const A*, void* = nullptr) { }

    typedef decltype(cpy_help_fun(static_cast<T*>(nullptr),
                                  static_cast<T*>(nullptr)))
            result_type;

 public:

    static const bool value = std::is_same<bool, result_type>::value;

};

template<typename T>
class is_copyable_<true, T>
{

 public:

    static const bool value = false;

};

} } // namespace cppa::detail

namespace cppa { namespace util {

template<typename T>
struct is_copyable
{

 public:

    static const bool value = detail::is_copyable_<std::is_abstract<T>::value, T>::value;

};

} } // namespace cppa::util

#endif // IS_COPYABLE_HPP
