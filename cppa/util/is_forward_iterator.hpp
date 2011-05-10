#ifndef IS_FORWARD_ITERATOR_HPP
#define IS_FORWARD_ITERATOR_HPP

#include "cppa/util/rm_ref.hpp"
#include "cppa/util/enable_if.hpp"
#include "cppa/util/disable_if.hpp"

namespace cppa { namespace util {

template<typename T>
class is_forward_iterator
{

    template<class C>
    static bool sfinae_fun
    (
        C* iter,
        // check for 'C::value_type C::operator*()' returning a non-void type
        typename rm_ref<decltype(*(*iter))>::type* = 0,
        // check for 'C& C::operator++()'
        typename enable_if<std::is_same<C&, decltype(++(*iter))>>::type* = 0,
        // check for 'bool C::operator==()'
        typename enable_if<std::is_same<bool, decltype(*iter == *iter)>>::type* = 0,
        // check for 'bool C::operator!=()'
        typename enable_if<std::is_same<bool, decltype(*iter != *iter)>>::type* = 0
    )
    {
        return true;
    }

    static void sfinae_fun(...) { }

    typedef decltype(sfinae_fun(static_cast<T*>(nullptr))) result_type;

 public:

    static const bool value = std::is_same<bool, result_type>::value;

};

} } // namespace cppa::util

#endif // IS_FORWARD_ITERATOR_HPP
