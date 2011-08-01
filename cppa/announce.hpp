#ifndef ANNOUNCE_HPP
#define ANNOUNCE_HPP

#include <typeinfo>

#include "cppa/uniform_type_info.hpp"
#include "cppa/util/abstract_uniform_type_info.hpp"
#include "cppa/detail/default_uniform_type_info_impl.hpp"

namespace cppa {

/**
 * @brief A simple example for @c announce with public accessible members.
 *
 * The output of this example program is:
 *
 * <tt>
 * foo(1,2)<br>
 * foo_pair(3,4)
 * </tt>
 * @example announce_example_1
 */

/**
 * @brief An example for @c announce with getter and setter member functions.
 *
 * The output of this example program is:
 *
 * <tt>foo(1,2)</tt>
 * @example announce_example_2
 */

/**
 * @brief An example for @c announce with overloaded
 *        getter and setter member functions.
 *
 * The output of this example program is:
 *
 * <tt>foo(1,2)</tt>
 * @example announce_example_3
 */

/**
 * @brief An example for @c announce with non-primitive members.
 *
 * The output of this example program is:
 *
 * <tt>bar(foo(1,2),3)</tt>
 * @example announce_example_4
 */

/**
 * @brief Adds a new type mapping to the type system.
 * @return @c true if @p uniform_type was added as known
 *         instance (mapped to @p plain_type); otherwise @c false
 *         is returned and @p uniform_type was deleted.
 */
bool announce(const std::type_info& tinfo, uniform_type_info* utype);

/**
 *
 */
template<class C, class Parent, typename... Args>
std::pair<C Parent::*, util::abstract_uniform_type_info<C>*>
compound_member(C Parent::*c_ptr, const Args&... args)
{
    return { c_ptr, new detail::default_uniform_type_info_impl<C>(args...) };
}

/**
 * @brief Adds a mapping for @p T to the type system.
 */
template<typename T, typename... Args>
inline bool announce(const Args&... args)
{
    return announce(typeid(T),
                    new detail::default_uniform_type_info_impl<T>(args...));
}

} // namespace cppa

#endif // ANNOUNCE_HPP
