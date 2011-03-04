#ifndef IS_LEGAL_TUPLE_TYPE_HPP
#define IS_LEGAL_TUPLE_TYPE_HPP

#include <type_traits>

namespace cppa { namespace util {

template<typename T>
struct is_legal_tuple_type
{
	static const bool value =    !std::is_pointer<T>::value
							  && !std::is_reference<T>::value;
};

} } // namespace cppa::util

#endif // IS_LEGAL_TUPLE_TYPE_HPP
