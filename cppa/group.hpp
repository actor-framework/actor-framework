#ifndef GROUP_HPP
#define GROUP_HPP

#include "cppa/channel.hpp"
#include "cppa/ref_counted.hpp"

namespace cppa {

class group : public channel
{

 protected:

	virtual void unsubscribe(const channel_ptr& who) = 0;

 public:

	class subscription;

	friend class group::subscription;

	// NOT thread safe
	class subscription : public ref_counted
	{

		channel_ptr m_self;
		intrusive_ptr<group> m_group;

		subscription() = delete;
		subscription(const subscription&) = delete;
		subscription& operator=(const subscription&) = delete;

	 public:

		subscription(const channel_ptr& s, const intrusive_ptr<group>& g);

		virtual ~subscription();

	};

	virtual intrusive_ptr<subscription> subscribe(const channel_ptr& who) = 0;

};

typedef intrusive_ptr<group> group_ptr;

} // namespace cppa

#endif // GROUP_HPP
