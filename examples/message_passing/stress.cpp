
#include "caf/actor_from_state.hpp"
#include "caf/behavior.hpp"
#include "caf/caf_main.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scoped_actor.hpp"

#include <chrono>
#include <iostream>

class aggregate_session {
public:
  aggregate_session(caf::event_based_actor* self, std::string city,
                    caf::actor destination)
    : self_(self),
      city_(std::move(city)),
      destination_(std::move(destination)) {
    // nop
  }
  caf::behavior make_behavior() {
    return {
      [=](double measurement) {
        total_volume_ += measurement;
        count_++;
      },
      // [=](std::string) {
      //   auto average = total_volume_ / static_cast<double>(count_);
      //   self_->send(destination_, city_, average);
      // }};
    };
  }

private:
  double total_volume_ = 0.0;
  size_t count_ = 0;
  caf::event_based_actor* self_;
  std::string city_;
  caf::actor destination_;
};

using namespace std::literals;

caf::behavior local_sensor(caf::event_based_actor* self, caf::actor other) {
  self->delayed_send<caf::message_priority::high>(self, 10s,
                                                  std::string{"Break"});
  self->send(self, 4.12);
  return {[=](double value) {
            for (int i = 0; i < 10; i++)
              self->delayed_send(other, 2ms, value);
            self->delayed_send(self, 2ms, value + 1);
          },
          [=](std::string) { self->quit(); }};
}

void caf_main(caf::actor_system& system) {
  for (int i = 0; i < 1000000; i++) {
    for (int i = 0; i < 10; i++) {
      auto agg = system.spawn(caf::actor_from_state<aggregate_session>,
                              std::string{"Tuzla"}, caf::actor{});
      for (int i = 0; i < 10; i++)
        system.spawn(local_sensor, agg);
    }
  }

  std::cout << "waiting" << std::endl;
  system.await_all_actors_done();
}

CAF_MAIN()
