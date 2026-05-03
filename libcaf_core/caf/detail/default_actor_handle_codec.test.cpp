// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/default_actor_handle_codec.hpp"

#include "caf/test/outline.hpp"
#include "caf/test/runnable.hpp"
#include "caf/test/scenario.hpp"

#include "caf/actor_cast.hpp"
#include "caf/actor_registry.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/json_reader.hpp"
#include "caf/json_writer.hpp"

#include <source_location>

using namespace caf;

namespace {

behavior testee_impl(event_based_actor*) {
  return behavior{
    [](int i) { return i; },
  };
}

class actor_scope_guard {
public:
  actor_scope_guard(actor hdl) : hdl_(hdl) {
    // nop
  }

  ~actor_scope_guard() {
    anon_send_exit(hdl_, exit_reason::kill);
  }

private:
  actor hdl_;
};

struct fixture {
  void select_serializer_by_name(std::string_view name) {
    if (name == "binary") {
      binary.emplace(bytes, &codec);
      return;
    }
    if (name == "json") {
      json.emplace(&codec);
      return;
    }
    CAF_RAISE_ERROR(std::logic_error, "invalid serializer name");
  }

  actor_system_config cfg;
  actor_system sys{cfg};
  detail::default_actor_handle_codec codec{sys};
  byte_buffer bytes;
  std::optional<binary_serializer> binary;
  std::optional<json_writer> json;

  template <class Fn>
  void with_writer(Fn&& fn, const std::source_location& loc
                            = std::source_location::current()) {
    auto& ctx = test::runnable::current();
    if (binary) {
      fn(*binary);
      return;
    }
    if (json) {
      fn(*json);
      return;
    }
    ctx.fail({"no format selected", loc});
  }

  template <class Fn>
  void with_reader(Fn&& fn, const std::source_location& loc
                            = std::source_location::current()) {
    auto& ctx = test::runnable::current();
    if (binary) {
      binary_deserializer reader{bytes, &codec};
      fn(reader);
      return;
    }
    if (json) {
      json_reader reader{&codec};
      if (!reader.load(json->str())) {
        ctx.fail({"failed to load JSON", loc});
      }
      fn(reader);
      return;
    }
    ctx.fail({"no format selected", loc});
  }
};

} // namespace

WITH_FIXTURE(fixture) {

OUTLINE("codecs roundtrip strong handles through the registry") {
  GIVEN("a strong actor pointer") {
    auto testee = sys.spawn(testee_impl);
    auto guard = actor_scope_guard{testee};
    auto ptr = actor_cast<strong_actor_ptr>(testee);
    WHEN("serializing the actor pointer as <format>") {
      select_serializer_by_name(block_parameters<std::string>());
      THEN("the actor pointer is put into the registry") {
        with_writer([this, &ptr](auto& writer) {
          check(writer.apply(ptr));
          check_eq(sys.registry().get(ptr->id()), ptr);
        });
      }
      AND_THEN("deserializing the actor obtains the same pointer") {
        with_reader([this, &ptr](auto& reader) {
          auto copy = strong_actor_ptr{};
          check(reader.apply(copy));
          check_eq(copy, ptr);
        });
      }
    }
  }
  EXAMPLES = R"_(
    | format |
    | binary |
    | json   |
  )_";
}

OUTLINE("codecs roundtrip valid weak handles through the registry") {
  GIVEN("a valid weak actor pointer") {
    auto testee = sys.spawn(testee_impl);
    auto guard = actor_scope_guard{testee};
    auto ptr = actor_cast<weak_actor_ptr>(testee);
    WHEN("serializing the actor pointer as <format>") {
      select_serializer_by_name(block_parameters<std::string>());
      THEN("the actor pointer is put into the registry") {
        with_writer([this, &ptr](auto& writer) {
          check(writer.apply(ptr));
          check_eq(sys.registry().get(ptr->id()), ptr);
        });
      }
      AND_THEN("deserializing the actor obtains the same pointer") {
        with_reader([this, &ptr](auto& reader) {
          auto copy = weak_actor_ptr{};
          check(reader.apply(copy));
          check_eq(copy, ptr);
        });
      }
    }
  }
  EXAMPLES = R"_(
    | format |
    | binary |
    | json   |
  )_";
}

OUTLINE("codecs serialize expired weak handles but bypass the registry") {
  GIVEN("an expired weak actor pointer") {
    auto testee = sys.spawn(testee_impl);
    auto ptr = actor_cast<weak_actor_ptr>(testee);
    testee = nullptr;
    WHEN("serializing the actor pointer as <format>") {
      select_serializer_by_name(block_parameters<std::string>());
      // If the user sends weak pointers, it will usually be as part of an exit
      // or down message. If the actor has already expired, we can't put it into
      // the registry since we can't form a strong pointer from it. We also
      // wouldn't want to store a weak pointer in the registry since that would
      // prevent reclaiming the memory. Hence, we do serialize it but don't put
      // it into the registry. If the actor is still known at the receiver, it
      // can deserialize it as usual. If the actor is no longer present in the
      // registry of the receiver, it will deserialize the handle as a nullptr.
      THEN("the actor pointer is *not* put into the registry") {
        with_writer([this, &ptr](auto& writer) {
          check(writer.apply(ptr));
          check_eq(sys.registry().get(ptr->id()), nullptr);
        });
      }
      AND_THEN("the serialized data contains actor ID and node ID") {
        with_reader([this, &ptr](auto& reader) {
          auto aid = actor_id{0};
          auto nid = node_id{};
          check(reader.template begin_object_t<weak_actor_ptr>());
          check(reader.begin_field("id"));
          check(reader.value(aid));
          check(reader.end_field());
          check(reader.begin_field("node"));
          check(inspect(reader, nid));
          check(reader.end_field());
          check(reader.end_object());
          const auto* cptr = ptr.ctrl();
          check_eq(aid, cptr->id());
          check_eq(nid, cptr->node());
        });
      }
      AND_THEN("deserializing the actor obtains a nullptr") {
        with_reader([this](auto& reader) {
          auto copy = weak_actor_ptr{};
          check(reader.apply(copy));
          check_eq(copy, nullptr);
        });
      }
    }
  }
  EXAMPLES = R"_(
    | format |
    | binary |
    | json   |
  )_";
}

} // WITH_FIXTURE(fixture)
