#include <cstdint>

#include "cppa/actor.hpp"
#include "cppa/serializer.hpp"

namespace cppa {

serializer::serializer(const intrusive_ptr<util::sink>& dsink) : m_sink(dsink)
{
}

serializer& operator<<(serializer& s, const actor_ptr&)
{
	return s;
}

serializer& operator<<(serializer& s, const std::string& str)
{
	auto str_size = static_cast<std::uint32_t>(str.size());
	s << str_size;
	s.write(str.size(), str.c_str());
	return s;
}

} // namespace cppa
