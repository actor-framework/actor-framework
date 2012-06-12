#ifndef IS_BUILTIN_HPP
#define IS_BUILTIN_HPP

#include <string>
#include <type_traits>

#include "cppa/atom.hpp"
#include "cppa/actor.hpp"
#include "cppa/group.hpp"
#include "cppa/channel.hpp"
#include "cppa/anything.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/process_information.hpp"

namespace cppa { class any_tuple; }
namespace cppa { namespace detail { class addressed_message; } }

namespace cppa { namespace util {

template<typename T>
struct is_builtin {
    static constexpr bool value = std::is_arithmetic<T>::value;
};

template<>
struct is_builtin<anything> : std::true_type { };

template<>
struct is_builtin<std::string> : std::true_type { };

template<>
struct is_builtin<std::u16string> : std::true_type { };

template<>
struct is_builtin<std::u32string> : std::true_type { };

template<>
struct is_builtin<atom_value> : std::true_type { };

template<>
struct is_builtin<any_tuple> : std::true_type { };

template<>
struct is_builtin<detail::addressed_message> : std::true_type { };

template<>
struct is_builtin<actor_ptr> : std::true_type { };

template<>
struct is_builtin<group_ptr> : std::true_type { };

template<>
struct is_builtin<channel_ptr> : std::true_type { };

template<>
struct is_builtin<intrusive_ptr<process_information> > : std::true_type { };

} } // namespace cppa::util

#endif // IS_BUILTIN_HPP
