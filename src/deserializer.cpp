#include <cstdint>

#include "cppa/actor.hpp"
#include "cppa/deserializer.hpp"

namespace cppa {

deserializer::deserializer(const intrusive_ptr<util::source>& src) : m_src(src)
{
}

} // namespace cppa

cppa::deserializer& operator>>(cppa::deserializer& d, cppa::actor_ptr&)
{
	return d;
}

cppa::deserializer& operator>>(cppa::deserializer& d, std::string& str)
{
	std::uint32_t str_size;
	d >> str_size;
	char* cbuf = new char[str_size + 1];
	d.read(str_size, cbuf);
	cbuf[str_size] = 0;
	str = cbuf;
	delete[] cbuf;
	return d;
}
