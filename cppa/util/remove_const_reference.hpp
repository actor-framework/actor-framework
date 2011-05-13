#ifndef REMOVE_CONST_REFERENCE_HPP
#define REMOVE_CONST_REFERENCE_HPP

namespace cppa { namespace util {

template<typename T>
struct remove_const_reference
{
    typedef T type;
};

template<typename T>
struct remove_const_reference<const T&>
{
    typedef T type;
};

} } // namespace cppa::util

#endif // REMOVE_CONST_REFERENCE_HPP
