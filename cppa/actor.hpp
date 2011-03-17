#ifndef ACTOR_HPP
#define ACTOR_HPP

#include "cppa/channel.hpp"

namespace cppa {

class actor : public channel
{

 public:

	virtual void link_to(const intrusive_ptr<actor>& other) = 0;

};

typedef intrusive_ptr<actor> actor_ptr;

} // namespace cppa

#endif // ACTOR_HPP
