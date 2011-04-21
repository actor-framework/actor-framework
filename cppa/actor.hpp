#ifndef ACTOR_HPP
#define ACTOR_HPP

#include "cppa/group.hpp"
#include "cppa/channel.hpp"

namespace cppa {

class serializer;
class deserializer;

class actor : public channel
{

 public:

	virtual void join(group_ptr& what) = 0;
	virtual void leave(const group_ptr& what) = 0;
	virtual void link(intrusive_ptr<actor>& other) = 0;
	virtual void unlink(intrusive_ptr<actor>& other) = 0;

	virtual bool remove_backlink(const intrusive_ptr<actor>& to) = 0;
	virtual bool establish_backlink(const intrusive_ptr<actor>& to) = 0;

};

typedef intrusive_ptr<actor> actor_ptr;

serializer& operator<<(serializer&, const actor_ptr&);

deserializer& operator>>(deserializer&, actor_ptr&);

} // namespace cppa

#endif // ACTOR_HPP
