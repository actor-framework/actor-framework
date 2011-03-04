#ifndef ACTOR_HPP
#define ACTOR_HPP

namespace cppa {

struct actor
{
	template<typename... Args>
	void send(const Args&...)
	{
	}
};

} // namespace cppa

#endif // ACTOR_HPP
