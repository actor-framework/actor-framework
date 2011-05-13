#ifndef ACTOR_BEHAVIOR_HPP
#define ACTOR_BEHAVIOR_HPP

namespace cppa {

class actor_behavior
{

 public:

    virtual ~actor_behavior();
    virtual void on_exit();
    virtual void act() = 0;

};

} // namespace cppa

#endif // ACTOR_BEHAVIOR_HPP
