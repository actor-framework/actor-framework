#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/async/publisher.hpp"
#include "caf/caf_main.hpp"
#include "caf/cow_vector.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scheduled_actor/flow.hpp"

#include <cstdint>
#include <future>
#include <iostream>

CAF_BEGIN_TYPE_ID_BLOCK(merge_test, caf::first_custom_type_id)
  CAF_ADD_TYPE_ID(merge_test, (caf::cow_vector<int>) )
CAF_END_TYPE_ID_BLOCK(merge_test)

using int_publisher = caf::async::publisher<int>;
using int_source = caf::async::consumer_resource<int>;
using int_sink = caf::async::producer_resource<int>;
using int_cow_vector = caf::cow_vector<int>;

auto apply_delay(caf::event_based_actor* self,
                 std::optional<caf::timespan> delay) {
  return [self, delay](auto&& src) {
    using src_t = std::decay_t<decltype(src)>;
    if (delay)
      std::forward<src_t>(src)
        .zip_with([](int value, int64_t) { return value; },
                  self->make_observable().interval(*delay))
        .as_observable();
    return std::forward<src_t>(src).as_observable();
  };
}

auto apply_limit(std::optional<size_t> limit) {
  return [limit](auto&& src) {
    if (limit)
      return std::move(src).take(*limit).as_observable();
    return std::move(src).as_observable();
  };
}

struct publisher {
  int init;
  size_t num;

  int_publisher make(caf::actor_system& sys) const {
    auto prom = std::promise<int_publisher>();
    auto future = prom.get_future();
    sys.spawn<caf::detached>(
      [cfg = *this, p = std::move(prom)](caf::event_based_actor* self) mutable {
        auto out = self //
                     ->make_observable()
                     .iota(cfg.init)
                     .take(cfg.num)
                     .to_publisher();
        p.set_value(std::move(out));
      });
    return future.get();
  }
};

template <class Inspector>
bool inspect(Inspector& f, publisher& x) {
  return f.object(x).fields(f.field("init", x.init), f.field("num", x.num));
}

struct subscriber {
  std::optional<size_t> limit;
  std::optional<caf::timespan> delay;

  std::future<int_cow_vector> start(caf::actor_system& sys,
                                    caf::flow::observable<int>& src) const {
    auto prom = std::promise<int_cow_vector>();
    auto res = prom.get_future();
    auto [self, launch] = sys.spawn_inactive<caf::event_based_actor>();
    src //
      .observe_on(self)
      .compose(apply_delay(self, delay))
      .compose(apply_limit(limit))
      .to_vector()
      .for_each([res = std::move(prom)](const int_cow_vector& xs) mutable {
        res.set_value(xs);
      });
    return res;
  }
};

template <class Inspector>
bool inspect(Inspector& f, subscriber& x) {
  return f.object(x).fields(f.field("limit", x.limit),
                            f.field("delay", x.delay));
}

struct config : caf::actor_system_config {
  config() {
    opt_group{custom_options_, "global"}
      .add(publishers, "publishers,p", "publishers to use")
      .add(subscribers, "subscribers,s", "subscribers to use");
  }

  std::vector<publisher> publishers;
  std::vector<subscriber> subscribers;
};

void print_compressed(const std::vector<int>& vec) {
  if (vec.empty()) {
    std::cout << "[]" << std::endl;
    return;
  }
  std::cout << "[" << vec[0];
  int start = vec[0];
  int prev = vec[0];
  for (size_t i = 1; i < vec.size(); ++i) {
    if (vec[i] == prev + 1) {
      prev = vec[i];
    } else {
      if (start != prev) {
        std::cout << "-" << prev;
      }
      std::cout << ", " << vec[i];
      start = prev = vec[i];
    }
  }
  if (start != prev) {
    std::cout << "-" << prev;
  }
  std::cout << "]";
}

void caf_main(caf::actor_system& sys, const config& cfg) {
  // Read the config.
  if (cfg.publishers.empty())
    CAF_RAISE_ERROR(std::runtime_error, "publishers invalid or empty");
  if (cfg.subscribers.empty())
    CAF_RAISE_ERROR(std::runtime_error, "subscribers invalid or empty");
  std::vector<std::future<int_cow_vector>> results;
  results.resize(cfg.subscribers.size());
  // Spin up the processing chain.
  sys.spawn<caf::detached>([&cfg, &results](caf::event_based_actor* self) {
    auto inputs = self->make_observable()
                    .from_container(cfg.publishers)
                    .map([self](const publisher& src) {
                      return src.make(self->system()).observe_on(self);
                    })
                    .merge()
                    .share(cfg.subscribers.size());
    for (size_t index = 0; index < cfg.subscribers.size(); index++) {
      results[index] = cfg.subscribers[index].start(self->system(), inputs);
    }
  });
  // Wait for the results and print them.
  sys.await_all_actors_done();
  for (size_t index = 0; index < results.size(); index++) {
    auto cow_xs = results[index].get();
    auto xs = std::move(cow_xs.unshared());
    std::sort(xs.begin(), xs.end());
    std::cout << "subscriber-" << index << ": ";
    print_compressed(xs);
    std::cout << std::endl;
  }
}

CAF_MAIN(caf::id_block::merge_test)
