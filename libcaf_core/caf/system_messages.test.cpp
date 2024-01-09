// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/system_messages.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"

using namespace caf;

WITH_FIXTURE(test::fixture::deterministic) {

TEST("exit_msg is comparable") {
  SECTION("invalid actor address") {
    auto msg1 = exit_msg{nullptr, sec::runtime_error};
    auto msg2 = exit_msg{nullptr, sec::runtime_error};
    auto msg3 = exit_msg{nullptr, sec::disposed};
    check_eq(msg1, msg2);
    check_eq(msg2, msg1);
    check_ne(msg1, msg3);
    check_ne(msg3, msg1);
  }
  SECTION("valid actor address") {
    auto dummy = sys.spawn([] { return behavior{[](int) {}}; });
    auto msg1 = exit_msg{dummy.address(), sec::runtime_error};
    auto msg2 = exit_msg{dummy.address(), sec::runtime_error};
    auto msg3 = exit_msg{dummy.address(), sec::disposed};
    check_eq(msg1, msg2);
    check_eq(msg2, msg1);
    check_ne(msg1, msg3);
    check_ne(msg3, msg1);
  }
}

TEST("exit_msg is serializable") {
  SECTION("invalid actor address") {
    auto msg1 = exit_msg{nullptr, sec::runtime_error};
    auto msg2 = serialization_roundtrip(msg1);
    check_eq(msg1, msg2);
  }
  SECTION("valid actor address") {
    // Note: serializing an actor puts it into the registry. Hence, we need to
    //       shut down the actor manually before the end because its reference
    //       count will not drop to zero otherwise.
    auto dummy = sys.spawn([] { return behavior{[](int) {}}; });
    auto dummy_guard = make_actor_scope_guard(dummy);
    auto msg1 = exit_msg{dummy.address(), sec::runtime_error};
    auto msg2 = serialization_roundtrip(msg1);
    check_eq(msg1, msg2);
  }
}

TEST("down_msg is comparable") {
  SECTION("invalid actor address") {
    auto msg1 = down_msg{nullptr, sec::runtime_error};
    auto msg2 = down_msg{nullptr, sec::runtime_error};
    auto msg3 = down_msg{nullptr, sec::disposed};
    check_eq(msg1, msg2);
    check_eq(msg2, msg1);
    check_ne(msg1, msg3);
    check_ne(msg3, msg1);
  }
  SECTION("valid actor address") {
    auto dummy = sys.spawn([] { return behavior{[](int) {}}; });
    auto msg1 = down_msg{dummy.address(), sec::runtime_error};
    auto msg2 = down_msg{dummy.address(), sec::runtime_error};
    auto msg3 = down_msg{dummy.address(), sec::disposed};
    check_eq(msg1, msg2);
    check_eq(msg2, msg1);
    check_ne(msg1, msg3);
    check_ne(msg3, msg1);
  }
}

TEST("down_msg is serializable") {
  SECTION("invalid actor address") {
    auto msg1 = down_msg{nullptr, sec::runtime_error};
    auto msg2 = serialization_roundtrip(msg1);
    check_eq(msg1, msg2);
  }
  SECTION("valid actor address") {
    // Note: serializing an actor puts it into the registry. Hence, we need to
    //       shut down the actor manually before the end because its reference
    //       count will not drop to zero otherwise.
    auto dummy = sys.spawn([] { return behavior{[](int) {}}; });
    auto dummy_guard = make_actor_scope_guard(dummy);
    auto msg1 = exit_msg{dummy.address(), sec::runtime_error};
    auto msg2 = serialization_roundtrip(msg1);
    check_eq(msg1, msg2);
  }
}

TEST("node_down_msg is comparable") {
  SECTION("empty node") {
    auto msg1 = node_down_msg{node_id{}, sec::runtime_error};
    auto msg2 = node_down_msg{node_id{}, sec::runtime_error};
    auto msg3 = node_down_msg{node_id{}, sec::disposed};
    check_eq(msg1, msg2);
    check_eq(msg2, msg1);
    check_ne(msg1, msg3);
  }
  SECTION("node from hashed_node_id") {
    auto msg1 = node_down_msg{node_id{hashed_node_id{}}, sec::runtime_error};
    auto msg2 = node_down_msg{node_id{hashed_node_id{}}, sec::runtime_error};
    auto msg3 = node_down_msg{node_id{hashed_node_id{}}, sec::disposed};
    check_eq(msg1, msg2);
    check_eq(msg2, msg1);
    check_ne(msg1, msg3);
  }
  SECTION("node from uri") {
    auto msg1 = node_down_msg{node_id{uri{}}, sec::runtime_error};
    auto msg2 = node_down_msg{node_id{uri{}}, sec::runtime_error};
    auto msg3 = node_down_msg{node_id{uri{}}, sec::disposed};
    check_eq(msg1, msg2);
    check_eq(msg2, msg1);
    check_ne(msg1, msg3);
  }
}

TEST("node_down_msg is serializable") {
  SECTION("empty node") {
    auto msg1 = node_down_msg{node_id{}, sec::runtime_error};
    auto msg2 = serialization_roundtrip(msg1);
    check_eq(msg1, msg2);
  }
  SECTION("node from hashed_node_id") {
    auto msg1 = node_down_msg{node_id{hashed_node_id{}}, sec::runtime_error};
    auto msg2 = serialization_roundtrip(msg1);
    check_eq(msg1, msg2);
  }
  SECTION("node from uri") {
    auto msg1 = node_down_msg{node_id{uri{}}, sec::runtime_error};
    auto msg2 = serialization_roundtrip(msg1);
    check_eq(msg1, msg2);
  }
}

TEST("stream_open_msg is serializable") {
  SECTION("invalid strong_actor_ptr") {
    auto msg1 = stream_open_msg{42, nullptr, 43};
    auto msg2 = serialization_roundtrip(msg1);
    if (check(msg2.has_value())) {
      check_eq(msg1.id, msg2->id);
      check_eq(msg1.sink, msg2->sink);
      check_eq(msg1.sink_flow_id, msg2->sink_flow_id);
    }
  }
  SECTION("valid strong_actor_ptr") {
    auto dummy = sys.spawn([] { return behavior{[](int) {}}; });
    auto dummy_guard = make_actor_scope_guard(dummy);
    auto dummy_abstract_ptr = actor_cast<abstract_actor*>(dummy);
    auto msg1
      = stream_open_msg{42, actor_control_block::from(dummy_abstract_ptr), 43};
    auto msg2 = serialization_roundtrip(msg1);
    if (check(msg2.has_value())) {
      check_eq(msg1.id, msg2->id);
      check_eq(msg1.sink, msg2->sink);
      check_eq(msg1.sink_flow_id, msg2->sink_flow_id);
    }
  }
}

TEST("stream_demand_msg is serializable") {
  auto msg1 = stream_demand_msg{42, 43};
  auto msg2 = serialization_roundtrip(msg1);
  if (check(msg2.has_value())) {
    check_eq(msg1.demand, msg2->demand);
    check_eq(msg1.source_flow_id, msg2->source_flow_id);
  }
}

TEST("stream_cancel_msg is serializable") {
  auto msg1 = stream_cancel_msg{42};
  auto msg2 = serialization_roundtrip(msg1);
  if (check(msg2.has_value()))
    check_eq(msg1.source_flow_id, msg2->source_flow_id);
}

TEST("stream_ack_msg is serializable") {
  SECTION("invalid strong_actor_ptr") {
    auto msg1 = stream_ack_msg{nullptr, 42, 43, 44};
    auto msg2 = serialization_roundtrip(msg1);
    if (check(msg2.has_value())) {
      check_eq(msg1.source, msg2->source);
      check_eq(msg1.source_flow_id, msg2->source_flow_id);
      check_eq(msg1.sink_flow_id, msg2->sink_flow_id);
      check_eq(msg1.max_items_per_batch, msg2->max_items_per_batch);
    }
  }
  SECTION("valid strong_actor_ptr") {
    auto dummy = sys.spawn([] { return behavior{[](int) {}}; });
    auto dummy_guard = make_actor_scope_guard(dummy);
    auto dummy_abstract_ptr = actor_cast<abstract_actor*>(dummy);
    auto msg1 = stream_ack_msg{actor_control_block::from(dummy_abstract_ptr),
                               42, 43, 44};
    auto msg2 = serialization_roundtrip(msg1);
    if (check(msg2.has_value())) {
      check_eq(msg1.source, msg2->source);
      check_eq(msg1.source_flow_id, msg2->source_flow_id);
      check_eq(msg1.sink_flow_id, msg2->sink_flow_id);
      check_eq(msg1.max_items_per_batch, msg2->max_items_per_batch);
    }
  }
}

TEST("stream_batch_msg is serializable") {
  SECTION("empty batch") {
    auto msg1 = stream_batch_msg{42, async::make_batch(std::vector<int>{})};
    auto msg2 = serialization_roundtrip(msg1);
    if (check(msg2.has_value())) {
      check_eq(msg1.sink_flow_id, msg2->sink_flow_id);
      check_eq(msg1.content.empty(), msg2->content.empty());
      check_eq(msg1.content.item_type(), msg2->content.item_type());
    }
  }
  SECTION("batch with data") {
    auto msg1 = stream_batch_msg{42,
                                 async::make_batch(std::vector<int>{1, 2, 3})};
    auto msg2 = serialization_roundtrip(msg1);
    if (check(msg2.has_value())) {
      check_eq(msg1.sink_flow_id, msg2->sink_flow_id);
      check_eq(msg1.content.size(), msg2->content.size());
      check_eq(msg1.content.item_type(), msg2->content.item_type());
    }
  }
}

TEST("stream_close_msg is serializable") {
  auto msg1 = stream_close_msg{42};
  auto msg2 = serialization_roundtrip(msg1);
  if (check(msg2.has_value()))
    check_eq(msg1.sink_flow_id, msg2->sink_flow_id);
}

TEST("stream_abort_msg is serializable") {
  auto msg1 = stream_abort_msg{42, sec::runtime_error};
  auto msg2 = serialization_roundtrip(msg1);
  if (check(msg2.has_value())) {
    check_eq(msg1.sink_flow_id, msg2->sink_flow_id);
    check_eq(msg1.reason, msg2->reason);
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)
