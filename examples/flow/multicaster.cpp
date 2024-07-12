// Non-interactive example to showcase the `multicaster`.

#include "caf/actor_from_state.hpp"
#include "caf/async/publisher.hpp"
#include "caf/caf_main.hpp"
#include "caf/cow_string.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/flow/backpressure_overflow_strategy.hpp"
#include "caf/flow/fwd.hpp"
#include "caf/scheduled_actor/flow.hpp"

#include <chrono>
#include <cstdint>
#include <random>
#include <string>

using namespace std::literals;

// Sends a random "measurement" to the collector once per second.
struct sensor_state {
  sensor_state(caf::event_based_actor* self, std::string name,
               caf::actor collector)
    : self(self),
      name(std::move(name)),
      collector(std::move(collector)),
      engine(std::random_device{}()) {
    // nop
  }

  caf::behavior make_behavior() {
    self->monitor(collector, [this](const caf::error& reason) {
      // Stop the sensor if the collector goes down.
      self->println("sensor {} lost its collector ({}) and shuts down", name,
                    reason);
      self->quit(reason);
    });
    self->mail(caf::update_atom_v).delay(1s).send(self);
    return {
      [this](caf::update_atom) {
        self->mail(caf::update_atom_v, name, dist(engine)).send(collector);
        self->mail(caf::update_atom_v).delay(1s).send(self);
      },
    };
  }

  caf::event_based_actor* self;
  std::string name;
  caf::actor collector;
  std::default_random_engine engine;
  std::uniform_int_distribution<int32_t> dist{-100, 100};
};

struct sensor_update {
  std::string name;
  int32_t value;
};

using sensor_update_ptr = std::shared_ptr<sensor_update>;

using sensor_update_publisher = caf::async::publisher<sensor_update_ptr>;

// Collects measurements from sensors and combines them into a single flow.
struct collector_state {
  // Constructs the state and initializes the output variable `pub`.
  collector_state(caf::event_based_actor* self, sensor_update_publisher* pub)
    : self(self), out(self) {
    // Connect our multicaster to the publisher.
    *pub = out.as_observable().to_publisher();
  }

  caf::behavior make_behavior() {
    return {
      [this](caf::update_atom, std::string name, int32_t value) {
        // Safety check: make sure our clients keep up with the incoming data.
        // In a real-world application, we would want to connect all clients to
        // a flow like this with the on_backpressure_buffer operator to make
        // sure that updates won't pile up in the collector.
        if (out.buffered() >= 100) {
          self->println("collector: buffer overflow");
          self->quit(caf::sec::backpressure_overflow);
          return;
        }
        if (!out.has_observers()) {
          // No need to process the update if no one is listening. Note that
          // this check is entirely optional: calling `push` on a multicaster
          // with no observers is a no-op.
          return;
        }
        auto ev = std::make_shared<sensor_update>();
        ev->name = std::move(name);
        ev->value = value;
        out.push(std::move(ev));
      },
    };
  }

  caf::event_based_actor* self;
  caf::flow::multicaster<sensor_update_ptr> out;
};

void caf_main(caf::actor_system& sys) {
  // Create the collector and get a publisher from it.
  sensor_update_publisher pub;
  auto collector = sys.spawn(caf::actor_from_state<collector_state>, &pub);
  // Create a few sensors.
  constexpr auto sensor_impl = caf::actor_from_state<sensor_state>;
  auto sensor1 = sys.spawn(sensor_impl, "sensor1", collector);
  auto sensor2 = sys.spawn(sensor_impl, "sensor2", collector);
  auto sensor3 = sys.spawn(sensor_impl, "sensor3", collector);
  // Subscribe to the flow, print the first 10 elements and then shut down.
  sys.spawn([pub, collector](caf::event_based_actor* self) {
    pub.observe_on(self)
      .filter([](const sensor_update_ptr& update) {
        // Make sure we only processes valid updates.
        return update != nullptr;
      })
      .take(10)
      .do_finally([self, collector] {
        // Shut down the collector once we're done. Otherwise, the program would
        // keep running indefinitely.
        self->send_exit(collector, caf::exit_reason::user_shutdown);
      })
      .for_each([self](const sensor_update_ptr& update) {
        self->println("received update from {}: {}", update->name,
                      update->value);
      });
  });
}

CAF_MAIN()
