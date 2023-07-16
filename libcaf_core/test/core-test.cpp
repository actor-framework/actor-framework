#define CAF_TEST_NO_MAIN

#include "caf/test/unit_test_impl.hpp"

#include "core-test.hpp"

#include <atomic>

namespace {

/// A trivial disposable with an atomic flag.
class trivial_impl : public caf::ref_counted, public caf::disposable::impl {
public:
  trivial_impl() : flag_(false) {
    // nop
  }

  void dispose() override {
    flag_ = true;
  }

  bool disposed() const noexcept override {
    return flag_.load();
  }

  void ref_disposable() const noexcept override {
    ref();
  }

  void deref_disposable() const noexcept override {
    deref();
  }

  friend void intrusive_ptr_add_ref(const trivial_impl* ptr) noexcept {
    ptr->ref();
  }

  friend void intrusive_ptr_release(const trivial_impl* ptr) noexcept {
    ptr->deref();
  }

private:
  std::atomic<bool> flag_;
};

} // namespace

namespace caf::flow {

std::string to_string(observer_state x) {
  switch (x) {
    default:
      return "???";
    case observer_state::idle:
      return "caf::flow::observer_state::idle";
    case observer_state::subscribed:
      return "caf::flow::observer_state::subscribed";
    case observer_state::completed:
      return "caf::flow::observer_state::completed";
    case observer_state::aborted:
      return "caf::flow::observer_state::aborted";
  }
}

bool from_string(std::string_view in, observer_state& out) {
  if (in == "caf::flow::observer_state::idle") {
    out = observer_state::idle;
    return true;
  }
  if (in == "caf::flow::observer_state::subscribed") {
    out = observer_state::subscribed;
    return true;
  }
  if (in == "caf::flow::observer_state::completed") {
    out = observer_state::completed;
    return true;
  }
  if (in == "caf::flow::observer_state::aborted") {
    out = observer_state::aborted;
    return true;
  }
  return false;
}

bool from_integer(std::underlying_type_t<observer_state> in,
                  observer_state& out) {
  auto result = static_cast<observer_state>(in);
  switch (result) {
    default:
      return false;
    case observer_state::idle:
    case observer_state::subscribed:
    case observer_state::completed:
    case observer_state::aborted:
      out = result;
      return true;
  }
}

disposable make_trivial_disposable() {
  return disposable{make_counted<trivial_impl>()};
}

void passive_subscription_impl::request(size_t n) {
  demand += n;
}

void passive_subscription_impl::dispose() {
  disposed_flag = true;
}

bool passive_subscription_impl::disposed() const noexcept {
  return disposed_flag;
}

} // namespace caf::flow

std::string to_string(level lvl) {
  switch (lvl) {
    case level::all:
      return "all";
    case level::trace:
      return "trace";
    case level::debug:
      return "debug";
    case level::warning:
      return "warning";
    case level::error:
      return "error";
    default:
      return "???";
  }
}

bool from_string(std::string_view str, level& lvl) {
  auto set = [&](level value) {
    lvl = value;
    return true;
  };
  if (str == "all")
    return set(level::all);
  else if (str == "trace")
    return set(level::trace);
  else if (str == "debug")
    return set(level::debug);
  else if (str == "warning")
    return set(level::warning);
  else if (str == "error")
    return set(level::error);
  else
    return false;
}

bool from_integer(uint8_t val, level& lvl) {
  if (val < 5) {
    lvl = static_cast<level>(val);
    return true;
  } else {
    return false;
  }
}

int main(int argc, char** argv) {
  using namespace caf;
  init_global_meta_objects<id_block::core_test>();
  core::init_global_meta_objects();
  return test::main(argc, argv);
}
