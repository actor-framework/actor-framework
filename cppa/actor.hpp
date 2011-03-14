#ifndef ACTOR_HPP
#define ACTOR_HPP

#include "cppa/message_receiver.hpp"

#include "cppa/detail/actor_public.hpp"
#include "cppa/detail/actor_private.hpp"

namespace cppa {

class actor : public message_receiver
{

	typedef message_receiver super;

 public:

	typedef cppa::intrusive_ptr<detail::actor_private> ptr_type;

	actor() = default;

	~actor();

	inline actor(detail::actor_private* ptr) : super(ptr) { }

	inline actor(const ptr_type& ptr) : super(ptr) { }

	inline actor(ptr_type&& ptr) : super(ptr) { }

	inline actor(const actor& other) : super(other) { }

	inline actor(actor&& other) : super(other) { }

	actor& operator=(const actor&);

	actor& operator=(actor&& other);

};

} // namespace cppa

#endif // ACTOR_HPP
