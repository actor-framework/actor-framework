// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/mtl.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/actor_from_state.hpp"
#include "caf/detail/assert.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/json_reader.hpp"
#include "caf/json_writer.hpp"
#include "caf/log/test.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/stateful_actor.hpp"
#include "caf/type_id.hpp"
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

template <class T>
struct kvp_field_name;

template <>
struct kvp_field_name<std::string> {
  static constexpr std::string_view value = "key";
};

template <>
struct kvp_field_name<int32_t> {
  static constexpr std::string_view value = "value";
};

template <class T>
constexpr std::string_view kvp_field_name_v = kvp_field_name<T>::value;

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
        auto& this_test = test::runnable::current();
        this_test.check(mtl.self() == self);
        this_test.check(std::addressof(mtl.reader()) == &reader);
        if (mode == "try_send") {
          this_test.check(mtl.try_send(kvs));
          log::test::debug("adapter generated: {}",
                           to_string(mtl.adapter().last_read));
          return make_message();
        } else {
          CAF_ASSERT(mode == "try_request");
          auto rp = self->make_response_promise();
          auto on_result = [this, rp, &this_test](auto&... xs) mutable {
            // Must receive either an int32_t or an empty message.
            if constexpr (sizeof...(xs) == 1) {
              this_test.check_eq(
                make_type_id_list<int32_t>(),
                make_type_id_list<std::decay_t<decltype(xs)>...>());
            } else {
              static_assert(sizeof...(xs) == 0);
            }
            // Convert input to JSON and fulfill the promise using the string.
            writer.reset();
            adapter{}.write(writer, xs...);
            rp.deliver(std::string{writer.str()});
          };
          auto on_error = [rp](error& err) mutable {
            rp.deliver(std::move(err));
          };
          this_test.check(mtl.try_request(kvs, infinite, on_result, on_error));
          log::test::debug("adapter generated: {}",
                           to_string(mtl.adapter().last_read));
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

struct fixture : test::fixture::deterministic {
  testee_actor testee;
  actor driver;

  scoped_actor self{sys};

  fixture() {
    testee = sys.spawn<lazy_init>(actor_from_state<testee_state>);
    driver = sys.spawn<lazy_init>(actor_from_state<driver_state>, testee);
  }
};

} // namespace

WITH_FIXTURE(fixture) {

SCENARIO("an MTL allows sending asynchronous messages") {
  GIVEN("a driver using an MTL to communicate to the testee") {
    WHEN("sending a JSON put message to the driver") {
      std::string put = R"({"@type": "caf::put_atom", "key": "a", "value": 1})";
      THEN("try_send generates a CAF put message to the testee") {
        inject().with(std::string{"try_send"}, put).from(self).to(driver);
        expect<put_atom, std::string, int32_t>()
          .with(put_atom_v, "a", 1)
          .from(driver)
          .to(testee);
        dispatch_messages();
        check_eq(mail_count(), 0u);
      }
    }
    AND_WHEN("send a JSON get message to the driver afterwards") {
      std::string get = R"({"@type": "caf::get_atom", "key": "a"})";
      THEN("try_send generates a CAF get message to the testee") {
        inject().with(std::string{"try_send"}, get).from(self).to(driver);
        expect<get_atom, std::string>()
          .with(get_atom_v, "a")
          .from(driver)
          .to(testee);
        expect<int32_t>().with(1).from(testee).to(driver);
        dispatch_messages();
        check_eq(mail_count(), 0u);
      }
    }
  }
}

SCENARIO("an MTL allows sending requests") {
  GIVEN("a driver using an MTL to communicate to the testee") {
    WHEN("sending a JSON put message to the driver") {
      std::string put = R"({"@type": "caf::put_atom", "key": "a", "value": 1})";
      THEN("try_request generates a CAF put message to the testee") {
        inject().with(std::string{"try_request"}, put).from(self).to(driver);
        expect<put_atom, std::string, int32_t>()
          .with(put_atom_v, "a", 1)
          .from(driver)
          .to(testee);
        dispatch_message();
        auto received_response = std::make_shared<bool>(false);
        self->receive([this, &received_response](std::string& response) {
          *received_response = true;
          check_eq(response, R"({"@type": "caf::unit_t"})");
        });
        check_eq(mail_count(), 0u);
        check(*received_response);
      }
    }
    AND_WHEN("send a JSON get message to the driver afterwards") {
      std::string get = R"({"@type": "caf::get_atom", "key": "a"})";
      THEN("try_request generates a CAF get message to the testee") {
        inject().with(std::string{"try_request"}, get).from(self).to(driver);
        expect<get_atom, std::string>()
          .with(get_atom_v, "a")
          .from(driver)
          .to(testee);
        expect<int32_t>().with(1).from(testee).to(driver);
        auto received_response = std::make_shared<bool>(false);
        self->receive([this, &received_response](std::string& response) {
          *received_response = true;
          check_eq(response, "1");
        });
        check_eq(mail_count(), 0u);
        check(*received_response);
      }
    }
  }
}

} // WITH_FIXTURE(fixture)
