#ifndef DESERIALIZER_HPP
#define DESERIALIZER_HPP

#include <string>
#include <cstddef>
#include <type_traits>

#include "cppa/any_type.hpp"
#include "cppa/intrusive_ptr.hpp"

#include "cppa/util/source.hpp"
#include "cppa/util/enable_if.hpp"

#include "cppa/detail/swap_bytes.hpp"

namespace cppa { namespace util {

struct void_type;

} } // namespace util

namespace cppa {

class deserializer
{

	intrusive_ptr<util::source> m_src;

 public:

	deserializer(const intrusive_ptr<util::source>& data_source);

	inline void read(std::size_t buf_size, void* buf)
	{
		m_src->read(buf_size, buf);
	}

};

deserializer& operator>>(deserializer&, std::string&);

template<typename T>
typename util::enable_if<std::is_integral<T>, deserializer&>::type
operator>>(deserializer& d, T& value)
{
	d.read(sizeof(T), &value);
	value = detail::swap_bytes(value);
	return d;
}

template<typename T>
typename util::enable_if<std::is_floating_point<T>, deserializer&>::type
operator>>(deserializer& d, T& value)
{
	d.read(sizeof(T), &value);
	return d;
}

inline deserializer& operator>>(deserializer& d, util::void_type&)
{
	return d;
}

inline deserializer& operator>>(deserializer& d, any_type&)
{
	return d;
}

} // namespace cppa

#endif // DESERIALIZER_HPP
