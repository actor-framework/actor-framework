#ifndef UNIFORM_TYPE_INFO_HPP
#define UNIFORM_TYPE_INFO_HPP

#include <map>
#include <string>
#include <cstdint>
#include <typeinfo>

#include "cppa/utype.hpp"
#include "cppa/object.hpp"
#include "cppa/serializer.hpp"
#include "cppa/deserializer.hpp"

#include "cppa/detail/utype_impl.hpp"
#include "cppa/detail/object_impl.hpp"

namespace cppa {

/**
 * @brief Get the uniform type information for @p T.
 */
template<typename T>
const utype& uniform_type_info()
{
	return detail::utype_impl<T>::instance;
}

/**
 * @brief Get the uniform type information associated with the name
 *        @p uniform_type_name.
 */
const utype& uniform_type_info(const std::string& uniform_type_name);

} // namespace cppa

#define CPPA_CONCAT_II(res) res
#define CPPA_CONCAT_I(lhs, rhs) CPPA_CONCAT_II(lhs ## rhs)
#define CPPA_CONCAT(lhs, rhs) CPPA_CONCAT_I( lhs, rhs )

#ifdef __GNUC__
#	define CPPA_ANNOUNCE(what)                                                 \
	static const std::uint8_t CPPA_CONCAT( __unused_val , __LINE__ )           \
			__attribute__ ((unused))                                           \
			= cppa::detail::utype_impl< what >::instance.announce_helper()
#else
#	define CPPA_ANNOUNCE(what)                                                 \
	static const std::uint8_t CPPA_CONCAT( __unused_val , __LINE__ )           \
			= cppa::detail::utype_impl< what >::instance.announce_helper()
#endif

/*
CPPA_ANNOUNCE(char);
CPPA_ANNOUNCE(std::int8_t);
CPPA_ANNOUNCE(std::uint8_t);
CPPA_ANNOUNCE(std::int16_t);
CPPA_ANNOUNCE(std::uint16_t);
CPPA_ANNOUNCE(std::int32_t);
CPPA_ANNOUNCE(std::uint32_t);
CPPA_ANNOUNCE(std::int64_t);
CPPA_ANNOUNCE(std::uint64_t);
CPPA_ANNOUNCE(float);
CPPA_ANNOUNCE(double);
CPPA_ANNOUNCE(long double);
CPPA_ANNOUNCE(std::string);
*/

#endif // UNIFORM_TYPE_INFO_HPP
