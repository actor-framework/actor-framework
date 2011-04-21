#ifndef SERIALIZER_HPP
#define SERIALIZER_HPP

#include <string>
#include <cstddef>
#include <type_traits>

#include "cppa/any_type.hpp"
#include "cppa/intrusive_ptr.hpp"

#include "cppa/util/sink.hpp"
#include "cppa/util/enable_if.hpp"

#include "cppa/detail/swap_bytes.hpp"

namespace cppa { namespace util {

struct void_type;

} } // namespace util

namespace cppa {

class serializer
{

	intrusive_ptr<util::sink> m_sink;

 public:

	serializer(const intrusive_ptr<util::sink>& data_sink);

	inline void write(std::size_t buf_size, const void* buf)
	{
		m_sink->write(buf_size, buf);
	}

	template<typename T>
	typename util::enable_if<std::is_integral<T>, void>::type
	write_int(const T& value)
	{
		T tmp = detail::swap_bytes(value);
		write(sizeof(T), &tmp);
	}

};

serializer& operator<<(serializer&, const std::string&);

template<typename T>
typename util::enable_if<std::is_integral<T>, serializer&>::type
operator<<(serializer& s, const T& value)
{
	s.write_int(value);
	return s;
}

template<typename T>
typename util::enable_if<std::is_floating_point<T>, serializer&>::type
operator<<(serializer& s, const T& value)
{
	s.write(sizeof(T), &value);
	return s;
}

inline serializer& operator<<(serializer& s, const util::void_type&)
{
	return s;
}

inline serializer& operator<<(serializer& s, const any_type&)
{
	return s;
}

} // namespace cppa

#endif // SERIALIZER_HPP
