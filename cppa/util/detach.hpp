#ifndef DETACH_HPP
#define DETACH_HPP

#include "cppa/util/is_copyable.hpp"
#include "cppa/util/has_copy_member_fun.hpp"

namespace cppa { namespace detail {

template<bool IsCopyable, bool HasCpyMemFun>
struct detach_;

template<bool HasCpyMemFun>
struct detach_<true, HasCpyMemFun>
{
	template<typename T>
	inline static T* cpy(const T* what)
	{
		return new T(*what);
	}
};

template<>
struct detach_<false, true>
{
	template<typename T>
	inline static T* cpy(const T* what)
	{
		return what->copy();
	}
};

} } // namespace cppa::detail

namespace cppa { namespace util {

template<typename T>
inline T* detach(const T* what)
{
	return detail::detach_<is_copyable<T>::value,
						   has_copy_member_fun<T>::value>::cpy(what);
}

} } // namespace cppa::util

#endif // DETACH_HPP
