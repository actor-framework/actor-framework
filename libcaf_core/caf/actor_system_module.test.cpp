// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/actor_system_module.hpp"

#include "caf/test/test.hpp"

using namespace caf;
using namespace std::literals;

namespace {

struct mock_module : actor_system_module {
  id_t ext_id;

  explicit mock_module(id_t id) : ext_id(id) {
    // nop
  }

  void start() override {
    // nop
  }

  void stop() override {
    // nop
  }

  void init(actor_system_config&) override {
    // nop
  }

  id_t id() const override {
    return ext_id;
  }

  void* subtype_ptr() override {
    return this;
  }
};

} // namespace

TEST("actor_system_module::name returns a string representation of the ID") {
  check_eq(mock_module{actor_system_module::middleman}.name(), "middleman"sv);
  check_eq(mock_module{actor_system_module::openssl_manager}.name(),
           "openssl-manager"sv);
  check_eq(mock_module{actor_system_module::network_manager}.name(),
           "network-manager"sv);
  check_eq(mock_module{actor_system_module::daemons}.name(), "daemons"sv);
  check_eq(mock_module{actor_system_module::extension1}.name(), "extension1"sv);
  check_eq(mock_module{actor_system_module::extension2}.name(), "extension2"sv);
  check_eq(mock_module{actor_system_module::extension3}.name(), "extension3"sv);
  check_eq(mock_module{actor_system_module::extension4}.name(), "extension4"sv);
}
