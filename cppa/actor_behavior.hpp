#ifndef ACTOR_BEHAVIOR_HPP
#define ACTOR_BEHAVIOR_HPP

namespace cppa {

class actor_behavior
{

 public:

	virtual void act() = 0;

	virtual void on_exit();

};

} // namespace cppa

#endif // ACTOR_BEHAVIOR_HPP
