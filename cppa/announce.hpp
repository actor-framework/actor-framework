#ifndef ANNOUNCE_HPP
#define ANNOUNCE_HPP

#include <typeinfo>

#include "cppa/uniform_type_info.hpp"
#include "cppa/util/uniform_type_info_base.hpp"
#include "cppa/detail/default_uniform_type_info_impl.hpp"

namespace cppa {

template<class C, class Parent, typename... Args>
std::pair<C Parent::*, util::uniform_type_info_base<C>*>
compound_member(C Parent::*c_ptr, const Args&... args)
{
    return { c_ptr, new detail::default_uniform_type_info_impl<C>(args...) };
}

/**
 * @brief Add a new type mapping to the libCPPA internal type system.
 * @return <code>true</code> if @p uniform_type was added as known
 *         instance (mapped to @p plain_type); otherwise @c false
 *         is returned and @p uniform_type was deleted.
 */
bool announce(const std::type_info& tinfo, uniform_type_info* utype);

template<typename T, typename... Args>
inline bool announce(const Args&... args)
{
    return announce(typeid(T),
                    new detail::default_uniform_type_info_impl<T>(args...));
}

} // namespace cppa

#endif // ANNOUNCE_HPP
