// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/async/blocking_producer.hpp"
#include "caf/flow/coordinator.hpp"
#include "caf/flow/observable.hpp"
#include "caf/flow/observable_builder.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/scoped_coordinator.hpp"

using namespace caf;
using namespace caf::flow;
using namespace std::literals;

namespace {

using ivec = std::vector<int>;

auto iota_vec(size_t n, int init = 1) {
  auto result = ivec{};
  result.resize(n);
  std::iota(result.begin(), result.end(), init);
  return result;
}

} // namespace

WITH_FIXTURE(test::fixture::flow) {

SCENARIO("repeater sources repeat one value indefinitely") {
  GIVEN("a repeater source") {
    WHEN("subscribing to its output") {
      THEN("the observer receives the same value over and over again") {
        using snk_t = passive_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        make_observable().repeat(42).subscribe(snk->as_observer());
        check_eq(snk->state, flow::observer_state::subscribed);
        check(snk->buf.empty());
        if (check(snk->subscribed())) {
          snk->sub.request(3);
          run_flows();
          check_eq(snk->buf, ivec({42, 42, 42}));
          snk->sub.request(4);
          run_flows();
          check_eq(snk->buf, ivec({42, 42, 42, 42, 42, 42, 42}));
          snk->sub.cancel();
          run_flows();
          check_eq(snk->buf, ivec({42, 42, 42, 42, 42, 42, 42}));
        }
      }
    }
  }
}

SCENARIO("container sources stream their input values") {
  GIVEN("a container source") {
    WHEN("subscribing to its output") {
      THEN("the observer receives the values from the container in order") {
        using snk_t = passive_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto xs = ivec{1, 2, 3, 4, 5, 6, 7};
        make_observable()
          .from_container(std::move(xs))
          .subscribe(snk->as_observer());
        check_eq(snk->state, flow::observer_state::subscribed);
        check(snk->buf.empty());
        if (check(snk->subscribed())) {
          snk->sub.request(3);
          run_flows();
          check_eq(snk->buf, ivec({1, 2, 3}));
          snk->sub.request(21);
          run_flows();
          check_eq(snk->buf, ivec({1, 2, 3, 4, 5, 6, 7}));
          check_eq(snk->state, flow::observer_state::completed);
        }
      }
    }
    WHEN("combining it with with a step that limits the amount of items") {
      THEN("the observer receives the defined subset of values") {
        using snk_t = flow::passive_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto xs = iota_vec(713);
        auto res = ivec{};
        check_eq(collect(
                   make_observable().from_container(std::move(xs)).take(678)),
                 iota_vec(678));
      }
    }
  }
}

SCENARIO("value sources produce exactly one input") {
  GIVEN("a value source") {
    WHEN("subscribing to its output") {
      THEN("the observer receives one value") {
        using snk_t = flow::passive_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        make_observable().just(42).subscribe(snk->as_observer());
        check_eq(snk->state, flow::observer_state::subscribed);
        check(snk->buf.empty());
        if (check(snk->subscribed())) {
          snk->sub.request(100);
          run_flows();
          check_eq(snk->buf, ivec({42}));
          check_eq(snk->state, flow::observer_state::completed);
        }
      }
    }
  }
}

SCENARIO("callable sources stream values generated from a function object") {
  GIVEN("a callable source returning non-optional values") {
    WHEN("subscribing to its output") {
      THEN("the observer receives an indefinite amount of values") {
        using snk_t = flow::passive_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto f = [n = 1]() mutable { return n++; };
        make_observable().from_callable(f).subscribe(snk->as_observer());
        check_eq(snk->state, flow::observer_state::subscribed);
        check(snk->buf.empty());
        if (check(snk->subscribed())) {
          snk->sub.request(3);
          run_flows();
          check_eq(snk->buf, ivec({1, 2, 3}));
          snk->sub.request(4);
          run_flows();
          check_eq(snk->buf, ivec({1, 2, 3, 4, 5, 6, 7}));
          snk->sub.cancel();
          run_flows();
          check_eq(snk->buf, ivec({1, 2, 3, 4, 5, 6, 7}));
        }
      }
    }
    WHEN("combining it with with a step that accepts a finite amount") {
      THEN("the observer receives a fixed amount of values") {
        auto res = ivec{};
        auto f = [n = 1]() mutable { return n++; };
        check_eq(collect(make_observable() //
                           .from_callable(f)
                           .take(713)),
                 iota_vec(713));
      }
    }
  }
  GIVEN("a callable source returning optional values") {
    WHEN("subscribing to its output") {
      THEN("the observer receives value until the callable return null-opt") {
        auto f = [n = 1]() mutable -> std::optional<int> {
          if (n < 8)
            return n++;
          else
            return std::nullopt;
        };
        using snk_t = flow::passive_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        make_observable().from_callable(f).subscribe(snk->as_observer());
        check_eq(snk->state, flow::observer_state::subscribed);
        check(snk->buf.empty());
        if (check(snk->subscribed())) {
          snk->sub.request(3);
          run_flows();
          check_eq(snk->buf, ivec({1, 2, 3}));
          snk->sub.request(21);
          run_flows();
          check_eq(snk->buf, ivec({1, 2, 3, 4, 5, 6, 7}));
          check_eq(snk->state, flow::observer_state::completed);
        }
      }
    }
    WHEN("combining it with with a step that accepts a finite amount") {
      THEN("the observer receives a fixed amount of values") {
        auto res = ivec{};
        auto f = [n = 1]() mutable -> std::optional<int> { return n++; };
        check_eq(collect(make_observable().from_callable(f).take(713)),
                 iota_vec(713));
      }
    }
  }
}

SCENARIO("callable sources must not be copyable") {
  GIVEN("a non-copyable callable source") {
    WHEN("subscribing to its output") {
      THEN("the observer receives all its items") {
        using snk_t = flow::passive_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto f = [n = 1, noncopyable = std::make_unique<int>(42)]() mutable {
          static_cast<void>(noncopyable);
          return n++;
        };
        make_observable()
          .from_callable(std::move(f))
          .subscribe(snk->as_observer());
        check_eq(snk->state, flow::observer_state::subscribed);
        check(snk->buf.empty());
        if (check(snk->subscribed())) {
          snk->sub.request(3);
          run_flows();
          check_eq(snk->buf, ivec({1, 2, 3}));
          snk->sub.request(4);
          run_flows();
          check_eq(snk->buf, ivec({1, 2, 3, 4, 5, 6, 7}));
          snk->sub.cancel();
          run_flows();
          check_eq(snk->buf, ivec({1, 2, 3, 4, 5, 6, 7}));
        }
      }
    }
  }
}

// This generator resembles a std::generator-like API that comes with a few
// difficulties:
// - The generator is not copyable.
// - The generator's iterator and sentinel types differ.
// - Getting an iterator requires a mutable generator.
// - The iterator has no post-increment operator.
// - The iterator is not default-constructible and not copyable.
struct iota_generator {
  using value_type = int;
  struct sentinel {};
  struct iterator {
    iterator(int current, int end) : current_(current), end_(end) {
    }

    iterator(const iterator&) = delete;
    iterator& operator=(const iterator&) = delete;

    bool operator==(sentinel) const {
      return current_ == end_;
    }

    bool operator!=(sentinel) const {
      return current_ != end_;
    }

    int operator*() const {
      return current_;
    }

    iterator& operator++() {
      ++current_;
      return *this;
    }

  private:
    int current_;
    const int end_;
  };

  explicit iota_generator(int end) : end_(end) {
  }

  iterator begin() {
    return {0, end_};
  }

  sentinel end() const {
    return {};
  }

private:
  const int end_ = 0;
};

SCENARIO("container sources must support generator-like APIs") {
  GIVEN("a generator-like container source") {
    WHEN("subscribing to its output") {
      THEN("the observer receives all its items") {
        using snk_t = flow::passive_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto f = iota_generator(100);
        make_observable()
          .from_container(std::move(f))
          .subscribe(snk->as_observer());
        check_eq(snk->state, flow::observer_state::subscribed);
        check(snk->buf.empty());
        if (check(snk->subscribed())) {
          snk->sub.request(3);
          run_flows();
          check_eq(snk->buf, ivec({0, 1, 2}));
          snk->sub.request(4);
          run_flows();
          check_eq(snk->buf, ivec({0, 1, 2, 3, 4, 5, 6}));
          snk->sub.cancel();
          run_flows();
          check_eq(snk->buf, ivec({0, 1, 2, 3, 4, 5, 6}));
        }
      }
    }
  }
}

SCENARIO("asynchronous buffers can generate flow items") {
  GIVEN("a background thread writing into an async buffer") {
    auto cancelled = std::atomic<bool>{false};
    auto producer_impl = [this, &cancelled](async::producer_resource<int> res) {
      auto producer = async::make_blocking_producer(std::move(res));
      if (!producer)
        fail("make_blocking_producer failed");
      for (int i = 1; i <= 713; ++i) {
        if (!producer->push(i)) {
          cancelled = true;
          return;
        }
      }
    };
    WHEN("reading all values from the buffer") {
      THEN("the observer receives all produced values") {
        auto [pull, push] = async::make_spsc_buffer_resource<int>();
        auto bg_thread = std::thread{producer_impl, push};
        auto res = ivec{};
        make_observable() //
          .from_resource(pull)
          .take(777)
          .for_each([&res](int val) { res.push_back(val); });
        run_flows(2s);
        check_eq(res, iota_vec(713));
        bg_thread.join();
        check(!cancelled);
      }
    }
    WHEN("reading only a subset of values from the buffer") {
      THEN("producer receives a cancel event after the selected items") {
        auto [pull, push] = async::make_spsc_buffer_resource<int>();
        auto bg_thread = std::thread{producer_impl, push};
        auto res = ivec{};
        make_observable() //
          .from_resource(pull)
          .take(20)
          .for_each([&res](int val) { res.push_back(val); });
        run_flows(2s);
        check_eq(res, iota_vec(20));
        bg_thread.join();
        check(cancelled);
      }
    }
    WHEN("canceling the subscription to the buffer") {
      THEN("the producer receives a cancel event") {
        auto [pull, push] = async::make_spsc_buffer_resource<int>();
        auto res = ivec{};
        auto sub = make_observable() //
                     .from_resource(pull)
                     .take(777)
                     .for_each([&res](int val) { res.push_back(val); });
        // Run initial actions to handle events from the initial request()
        // calls. Without this step, from_resource is in `running_` state and
        // we won't hit the code paths for disposing a "cold" object. This is
        // also why we spin up the thread later: making sure we're hitting the
        // code paths we want to test here.
        run_flows();
        sub.dispose();
        auto bg_thread = std::thread{producer_impl, push};
        run_flows();
        check(res.empty());
        bg_thread.join();
        check(cancelled);
      }
    }
  }
  GIVEN("a null-resource") {
    WHEN("trying to read from it") {
      THEN("the observer receives an error") {
        auto res = ivec{};
        auto err = error{};
        auto pull = async::consumer_resource<int>{};
        make_observable() //
          .from_resource(pull)
          .take(713)
          .do_on_error([&err](const error& what) { err = what; })
          .for_each([&res](int val) { res.push_back(val); });
        run_flows();
        check(res.empty());
        check(!err.empty());
      }
    }
  }
  GIVEN("a resource that has already been accessed") {
    WHEN("trying to read from it") {
      THEN("the observer receives an error") {
        auto [pull, push] = async::make_spsc_buffer_resource<int>();
        auto pull_cpy = pull;
        auto buf = pull_cpy.try_open();
        check(buf != nullptr);
        auto res = ivec{};
        auto err = error{};
        make_observable() //
          .from_resource(pull)
          .take(713)
          .do_on_error([&err](const error& what) { err = what; })
          .for_each([&res](int val) { res.push_back(val); });
        run_flows();
        check(res.empty());
        check(!err.empty());
      }
    }
  }
  GIVEN("a from_resource_sub object") {
    WHEN("manipulating its ref count as consumer or disposable") {
      THEN("the different pointer types manipulate the same ref count") {
        using buf_t = async::spsc_buffer<int>;
        using impl_t = op::from_resource_sub<buf_t>;
        using snk_t = flow::auto_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto ptr = make_counted<impl_t>(coordinator(), nullptr,
                                        snk->as_observer());
        check_eq(ptr->get_reference_count(), 1u);
        {
          auto sub = caf::flow::subscription{ptr.get()};
          check_eq(ptr->get_reference_count(), 2u);
        }
        run_flows(); // clean up the subscription
        check_eq(ptr->get_reference_count(), 1u);
        {
          auto cptr = async::consumer_ptr{ptr.get()};
          check_eq(ptr->get_reference_count(), 2u);
        }
        run_flows(); // clean up the subscription
        check_eq(ptr->get_reference_count(), 1u);
      }
    }
  }
}

namespace {

// Generates 7 integers and then calls on_complete.
class i7_generator {
public:
  using output_type = int;

  template <class Step, class... Steps>
  void pull(size_t n, Step& step, Steps&... steps) {
    for (size_t i = 0; i < n; ++i) {
      if (value_ > 7) {
        step.on_complete(steps...);
        return;
      } else if (!step.on_next(value_++, steps...)) {
        return;
      }
    }
  }

private:
  int value_ = 1;
};

// Generates 3 integers and then calls on_error.
class broken_generator {
public:
  using output_type = int;

  template <class Step, class... Steps>
  void pull(size_t n, Step& step, Steps&... steps) {
    for (size_t i = 0; i < n; ++i) {
      if (value_ > 3) {
        auto err = make_error(sec::runtime_error, "something went wrong");
        step.on_error(err, steps...);
        return;
      }
      if (!step.on_next(value_++, steps...))
        return;
    }
  }

private:
  int value_ = 1;
};

} // namespace

SCENARIO("users can provide custom generators") {
  GIVEN("an implementation of the generator concept") {
    WHEN("subscribing to its output") {
      THEN("the observer receives the generated values") {
        using snk_t = flow::passive_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto f = i7_generator{};
        make_observable().from_generator(f).subscribe(snk->as_observer());
        check_eq(snk->state, flow::observer_state::subscribed);
        check(snk->buf.empty());
        check(snk->subscribed());
        snk->request(3);
        run_flows();
        check_eq(snk->buf, ivec({1, 2, 3}));
        snk->sub.request(21);
        run_flows();
        check_eq(snk->buf, ivec({1, 2, 3, 4, 5, 6, 7}));
        check(snk->completed());
      }
    }
  }
  GIVEN("an implementation of the generator concept that calls on_error") {
    WHEN("subscribing to its output") {
      THEN("the observer receives the generated values followed by an error") {
        using snk_t = passive_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto f = broken_generator{};
        coordinator()->make_observable().from_generator(f).subscribe(
          snk->as_observer());
        check_eq(snk->state, flow::observer_state::subscribed);
        check(snk->buf.empty());
        check(snk->subscribed());
        snk->request(27);
        run_flows();
        check_eq(snk->buf, ivec({1, 2, 3}));
        if (check(snk->aborted())) {
          check_eq(snk->err, caf::sec::runtime_error);
        }
      }
    }
  }
}

} // WITH_FIXTURE(test::fixture::flow)
