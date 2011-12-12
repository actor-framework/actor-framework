#ifndef ACTOR_BEHAVIOR_HPP
#define ACTOR_BEHAVIOR_HPP

namespace cppa {

class scheduled_actor
{

 public:

    virtual ~scheduled_actor();
    virtual void on_exit();
    virtual void act() = 0;

};

} // namespace cppa

#endif // ACTOR_BEHAVIOR_HPP
