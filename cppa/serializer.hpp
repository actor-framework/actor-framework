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

} } // namespace cppa::util

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

};

} // namespace cppa

cppa::serializer& operator<<(cppa::serializer&, const std::string&);

template<typename T>
typename cppa::util::enable_if<std::is_integral<T>, cppa::serializer&>::type
operator<<(cppa::serializer& s, const T& value)
{
	T tmp = cppa::detail::swap_bytes(value);
	s.write(sizeof(T), &tmp);
	return s;
}

template<typename T>
typename cppa::util::enable_if<std::is_floating_point<T>,
							   cppa::serializer&>::type
operator<<(cppa::serializer& s, const T& value)
{
	s.write(sizeof(T), &value);
	return s;
}

inline cppa::serializer& operator<<(cppa::serializer& s,
									const cppa::util::void_type&)
{
	return s;
}

inline cppa::serializer& operator<<(cppa::serializer& s, const cppa::any_type&)
{
	return s;
}

#endif // SERIALIZER_HPP
