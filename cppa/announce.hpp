#ifndef ANNOUNCE_HPP
#define ANNOUNCE_HPP

#include <typeinfo>

#include "cppa/uniform_type_info.hpp"
#include "cppa/util/abstract_uniform_type_info.hpp"
#include "cppa/detail/default_uniform_type_info_impl.hpp"

namespace cppa {

/**
 * @addtogroup TypeSystem
 * @{
 */

/**
 * @brief A simple example for @c announce with public accessible members.
 *
 * The output of this example program is:
 *
 * <tt>
 * foo(1,2)<br>
 * foo_pair(3,4)
 * </tt>
 * @example announce_example_1.cpp
 */

/**
 * @brief An example for @c announce with getter and setter member functions.
 *
 * The output of this example program is:
 *
 * <tt>foo(1,2)</tt>
 * @example announce_example_2.cpp
 */

/**
 * @brief An example for @c announce with overloaded
 *        getter and setter member functions.
 *
 * The output of this example program is:
 *
 * <tt>foo(1,2)</tt>
 * @example announce_example_3.cpp
 */

/**
 * @brief An example for @c announce with non-primitive members.
 *
 * The output of this example program is:
 *
 * <tt>bar(foo(1,2),3)</tt>
 * @example announce_example_4.cpp
 */

/**
 * @brief An advanced example for @c announce implementing serialization
 *        for a user-defined tree data type.
 * @example announce_example_5.cpp
 */

/**
 * @brief Adds a new type mapping to the libcppa type system.
 * @param tinfo C++ RTTI for the new type
 * @param utype Corresponding {@link uniform_type_info} instance.
 * @returns @c true if @p uniform_type was added as known
 *         instance (mapped to @p plain_type); otherwise @c false
 *         is returned and @p uniform_type was deleted.
 */
bool announce(std::type_info const& tinfo, uniform_type_info* utype);

// deals with member pointer
/**
 * @brief Creates meta informations for a non-trivial member @p C.
 * @param c_ptr Pointer to the non-trivial member.
 * @param args "Sub-members" of @p c_ptr
 * @see {@link announce_example_4.cpp announce example 4}
 * @returns A pair of @p c_ptr and the created meta informations.
 */
template<class C, class Parent, typename... Args>
std::pair<C Parent::*, util::abstract_uniform_type_info<C>*>
compound_member(C Parent::*c_ptr, Args const&... args)
{
    return { c_ptr, new detail::default_uniform_type_info_impl<C>(args...) };
}

// deals with getter returning a mutable reference
template<class C, class Parent, typename... Args>
std::pair<C& (Parent::*)(), util::abstract_uniform_type_info<C>*>
compound_member(C& (Parent::*c_ptr)(), Args const&... args)
{
    return { c_ptr, new detail::default_uniform_type_info_impl<C>(args...) };
}

// deals with getter/setter pair
template<class Parent, typename GRes,
         typename SRes, typename SArg, typename... Args>
std::pair<std::pair<GRes (Parent::*)() const, SRes (Parent::*)(SArg)>,
          util::abstract_uniform_type_info<typename util::rm_ref<GRes>::type>*>
compound_member(const std::pair<GRes (Parent::*)() const, SRes (Parent::*)(SArg)>& gspair,
                Args const&... args)
{
    return { gspair, new detail::default_uniform_type_info_impl<typename util::rm_ref<GRes>::type>(args...) };
}


/**
 * @brief Adds a new type mapping for @p T to the libcppa type system.
 * @param args Members of @p T.
 * @returns @c true if @p T was added to the libcppa type system;
 *         otherwise @c false.
 */
template<typename T, typename... Args>
inline bool announce(Args const&... args)
{
    return announce(typeid(T),
                    new detail::default_uniform_type_info_impl<T>(args...));
}

/**
 * @}
 */

} // namespace cppa

#endif // ANNOUNCE_HPP
