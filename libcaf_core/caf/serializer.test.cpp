// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/serializer.hpp"

#include "caf/test/test.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"

#include <vector>

using namespace caf;

namespace {

class test_serializer : public serializer {
  // -- constructors, destructors, and assignment operators --------------------
public:
  using super = serializer;

  test_serializer(actor_system& sys, bool state) : super(sys), state_(state) {
    // nop
  }

  ~test_serializer() override {
    // nop
  }

  // -- interface functions ----------------------------------------------------

  bool begin_object(type_id_t, std::string_view) override {
    return state_;
  };

  bool end_object() override {
    return state_;
  };

  bool begin_field(std::string_view) override {
    return state_;
  };

  bool begin_field(std::string_view, bool) override {
    return state_;
  };

  bool begin_field(std::string_view, span<const type_id_t>, size_t) override {
    return state_;
  };

  bool begin_field(std::string_view, bool, span<const type_id_t>,
                   size_t) override {
    return state_;
  };

  bool end_field() override {
    return state_;
  };

  bool begin_tuple(size_t) override {
    return state_;
  };

  bool end_tuple() override {
    return state_;
  };

  bool begin_sequence(size_t size) override {
    return size % 2 == 0;
  };

  bool end_sequence() override {
    return state_;
  };

  bool value(std::byte) override {
    return state_;
  };

  bool value(bool) override {
    return state_;
  };

  bool value(int8_t) override {
    return state_;
  };

  bool value(uint8_t) override {
    return state_;
  };

  bool value(int16_t) override {
    return state_;
  };

  bool value(uint16_t) override {
    return state_;
  };

  bool value(int32_t) override {
    return state_;
  };

  bool value(uint32_t) override {
    return state_;
  };

  bool value(int64_t) override {
    return state_;
  };

  bool value(uint64_t) override {
    return state_;
  };

  bool value(float) override {
    return state_;
  };

  bool value(double) override {
    return state_;
  };

  bool value(long double) override {
    return state_;
  };

  bool value(std::string_view) override {
    return state_;
  };

  bool value(const std::u16string&) override {
    return state_;
  };

  bool value(const std::u32string&) override {
    return state_;
  };

  bool value(span<const std::byte>) override {
    return state_;
  };

private:
  bool state_ = false;
};

} // namespace

TEST("base serializer") {
  auto cfg = actor_system_config{};
  auto sys = actor_system{cfg};
  SECTION("failing serializer") {
    auto serializer = test_serializer{sys, false};
    check(!serializer.begin_associative_array(3));
    check(!serializer.end_associative_array());
    check(!serializer.begin_key_value_pair());
    check(!serializer.end_key_value_pair());
    check(!serializer.list(std::vector<bool>{true, false}));
  }
  SECTION("successful serializer") {
    auto serializer = test_serializer{sys, true};
    check(serializer.begin_associative_array(4));
    check(serializer.end_associative_array());
    check(serializer.begin_key_value_pair());
    check(serializer.end_key_value_pair());
    check(!serializer.list(std::vector<bool>{true, false, true}));
    check(serializer.list(std::vector<bool>{true, false}));
  }
}
