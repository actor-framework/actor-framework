#ifndef IMPLICIT_CONVERSIONS_HPP
#define IMPLICIT_CONVERSIONS_HPP

#include <string>
#include <type_traits>

#include "cppa/actor.hpp"

#include "cppa/util/is_array_of.hpp"
#include "cppa/util/replace_type.hpp"

namespace cppa { class local_actor; }

namespace cppa { namespace detail {

template<typename T>
struct implicit_conversions
{
    typedef typename util::replace_type<T, std::string,
                                        std::is_same<T, const char*>,
                                        std::is_same<T, char*>,
                                        util::is_array_of<T, char>,
                                        util::is_array_of<T, const char> >::type
            subtype1;

    typedef typename util::replace_type<subtype1, std::u16string,
                                        std::is_same<subtype1, const char16_t*>,
                                        std::is_same<subtype1, char16_t*>,
                                        util::is_array_of<subtype1, char16_t>>::type
            subtype2;

    typedef typename util::replace_type<subtype2, std::u32string,
                                        std::is_same<subtype2, const char32_t*>,
                                        std::is_same<subtype2, char32_t*>,
                                        util::is_array_of<subtype2, char32_t>>::type
            subtype3;

    typedef typename util::replace_type<subtype3, actor_ptr,
                                        std::is_same<actor*,T>,
                                        std::is_same<local_actor*,T>>::type
            type;
};

} } // namespace cppa::detail

#endif // IMPLICIT_CONVERSIONS_HPP
