#include "cppa/actor.hpp"

namespace cppa {

actor& actor::operator=(const actor& other)
{
	static_cast<super&>(*this) = other;
	return *this;
}

actor& actor::operator=(actor&& other)
{
	static_cast<super&>(*this) = other;
	return *this;
}

} // namespace cppa
