// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE mtl

#include "caf/mtl.hpp"

#include "core-test.hpp"

#include "caf/json_reader.hpp"
#include "caf/json_writer.hpp"
#include "caf/typed_actor.hpp"
#include "caf/typed_event_based_actor.hpp"

using namespace caf;

namespace {

using testee_actor = typed_actor<result<void>(put_atom, std::string, int32_t),
                                 result<int32_t>(get_atom, std::string)>;

struct testee_state {
  static inline const char* name = "testee";

  std::map<std::string, std::int32_t> kv_store;

  testee_actor::behavior_type make_behavior() {
    return {
      [this](put_atom, const std::string& key, int32_t val) {
        kv_store[key] = val;
      },
      [this](get_atom, const std::string& key) -> result<int32_t> {
        if (auto i = kv_store.find(key); i != kv_store.end())
          return {i->second};
        else
          return {make_error(sec::runtime_error, "key not found")};
      },
    };
  }
};

using testee_impl = testee_actor::stateful_impl<testee_state>;

template <class T>
struct kvp_field_name;

template <>
struct kvp_field_name<std::string> {
  static constexpr string_view value = "key";
};

template <>
struct kvp_field_name<int32_t> {
  static constexpr string_view value = "value";
};

template <class T>
constexpr string_view kvp_field_name_v = kvp_field_name<T>::value;

// Adapter for converting atom-prefixed message to pseudo-objects.
struct adapter {
  template <class Inspector, class Atom, class... Ts>
  bool read(Inspector& f, Atom, Ts&... xs) {
    auto type_annotation = type_name_v<Atom>;
    if (f.assert_next_object_name(type_annotation)
        && f.virtual_object(type_annotation)
             .fields(f.field(kvp_field_name_v<Ts>, xs)...)) {
      last_read = make_type_id_list<Atom, Ts...>();
      return true;
    } else {
      return false;
    }
  }

  template <class Inspector>
  bool write(Inspector& f, int32_t result) {
    return f.apply(result);
  }

  template <class Inspector>
  bool write(Inspector& f) {
    return f.apply(unit);
  }

  // Stores the type IDs for the last successful read.
  type_id_list last_read = make_type_id_list();
};

struct driver_state {
  static inline const char* name = "driver";

  event_based_actor* self;

  testee_actor kvs;

  json_reader reader;

  json_writer writer;

  driver_state(event_based_actor* self, testee_actor kvs)
    : self(self), kvs(std::move(kvs)) {
    // nop
  }

  behavior make_behavior() {
    return {
      [this](const std::string& mode,
             const std::string& json_text) -> result<message> {
        reader.load(json_text);
        auto mtl = make_mtl(self, adapter{}, &reader);
        CHECK(mtl.self() == self);
        CHECK(std::addressof(mtl.reader()) == &reader);
        if (mode == "try_send") {
          CHECK(mtl.try_send(kvs));
          MESSAGE("adapter generated: " << mtl.adapter().last_read);
          return make_message();
        } else {
          CAF_ASSERT(mode == "try_request");
          auto rp = self->make_response_promise();
          auto on_result = [this, rp](auto&... xs) mutable {
            // Must receive either an int32_t or an empty message.
            if constexpr (sizeof...(xs) == 1) {
              CHECK_EQ(make_type_id_list<int32_t>(),
                       make_type_id_list<std::decay_t<decltype(xs)>...>());
            } else {
              static_assert(sizeof...(xs) == 0);
            }
            // Convert input to JSON and fulfill the promise using the string.
            writer.reset();
            adapter{}.write(writer, xs...);
            rp.deliver(to_string(writer.str()));
          };
          auto on_error = [rp](error& err) mutable {
            rp.deliver(std::move(err));
          };
          CHECK(mtl.try_request(kvs, infinite, on_result, on_error));
          MESSAGE("adapter generated: " << mtl.adapter().last_read);
          return rp;
        }
      },
      [](int32_t) {
        // nop
      },
    };
  }
};

using driver_impl = stateful_actor<driver_state>;

struct fixture : test_coordinator_fixture<> {
  testee_actor testee;

  actor driver;

  fixture() {
    testee = sys.spawn<testee_impl, lazy_init>();
    driver = sys.spawn<driver_impl, lazy_init>(testee);
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("an MTL allows sending asynchronous messages") {
  GIVEN("a driver using an MTL to communicate to the testee") {
    WHEN("sending a JSON put message to the driver") {
      std::string put = R"({"@type": "caf::put_atom", "key": "a", "value": 1})";
      THEN("try_send generates a CAF put message to the testee") {
        inject((std::string, std::string),
               from(self).to(driver).with("try_send", put));
        expect((put_atom, std::string, int32_t),
               from(driver).to(testee).with(_, "a", 1));
        CHECK(!sched.has_job());
      }
    }
    WHEN("send a JSON get message to the driver afterwards") {
      std::string get = R"({"@type": "caf::get_atom", "key": "a"})";
      THEN("try_send generates a CAF get message to the testee") {
        inject((std::string, std::string),
               from(self).to(driver).with("try_send", get));
        expect((get_atom, std::string), from(driver).to(testee).with(_, "a"));
        expect((int32_t), from(testee).to(driver).with(_, 1));
        CHECK(!sched.has_job());
      }
    }
  }
}

SCENARIO("an MTL allows sending requests") {
  GIVEN("a driver using an MTL to communicate to the testee") {
    WHEN("sending a JSON put message to the driver") {
      std::string put = R"({"@type": "caf::put_atom", "key": "a", "value": 1})";
      THEN("try_request generates a CAF put message to the testee") {
        inject((std::string, std::string),
               from(self).to(driver).with("try_request", put));
        expect((put_atom, std::string, int32_t),
               from(driver).to(testee).with(_, "a", 1));
        expect((void), from(testee).to(driver));
        expect((std::string),
               from(driver).to(self).with(R"({"@type": "caf::unit_t"})"));
        CHECK(!sched.has_job());
      }
    }
    WHEN("send a JSON get message to the driver afterwards") {
      std::string get = R"({"@type": "caf::get_atom", "key": "a"})";
      THEN("try_request generates a CAF get message to the testee") {
        inject((std::string, std::string),
               from(self).to(driver).with("try_request", get));
        expect((get_atom, std::string), from(driver).to(testee).with(_, "a"));
        expect((int32_t), from(testee).to(driver).with(_, 1));
        expect((std::string), from(driver).to(self).with("1"));
        CHECK(!sched.has_job());
      }
    }
  }
}

END_FIXTURE_SCOPE()
