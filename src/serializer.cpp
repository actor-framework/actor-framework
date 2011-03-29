#include <cstdint>

#include "cppa/actor.hpp"
#include "cppa/serializer.hpp"

namespace cppa {

serializer::serializer(const intrusive_ptr<util::sink>& dsink) : m_sink(dsink)
{
}

} // namespace cppa

cppa::serializer& operator<<(cppa::serializer& s, const cppa::actor_ptr& aptr)
{
	return s;
}

cppa::serializer& operator<<(cppa::serializer& s, const std::string& str)
{
	auto str_size = static_cast<std::uint32_t>(str.size());
	s << str_size;
	s.write(str.size(), str.c_str());
	return s;
}
