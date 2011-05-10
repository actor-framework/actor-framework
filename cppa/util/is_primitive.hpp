#ifndef IS_PRIMITIVE_HPP
#define IS_PRIMITIVE_HPP

#include "cppa/detail/type_to_ptype.hpp"

namespace cppa { namespace util {

/**
 * @brief Evaluates to @c true if @c T is a primitive type.
 *
 * <code>is_primitive<T>::value == true</code> if and only if @c T
 * is a signed / unsigned integer or one of the following types:
 * - @c float
 * - @c double
 * - @c long @c double
 * - @c std::string
 * - @c std::u16string
 * - @c std::u32string
 */
template<typename T>
struct is_primitive
{
    static const bool value = detail::type_to_ptype<T>::ptype != pt_null;
};

} } // namespace cppa::util

#endif // IS_PRIMITIVE_HPP
