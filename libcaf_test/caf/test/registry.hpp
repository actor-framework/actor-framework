// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/test/block_type.hpp"
#include "caf/test/factory.hpp"
#include "caf/test/fwd.hpp"

#include "caf/detail/test_export.hpp"
#include "caf/unordered_flat_map.hpp"

#include <cstddef>
#include <map>
#include <string_view>

namespace caf::test {

/// A registry for our factories.
class CAF_TEST_EXPORT registry {
public:
  using void_function = void (*)();

  struct init_callback {
    init_callback* next;
    void_function callback;
  };

  constexpr registry() noexcept = default;

  registry(const registry&) = delete;

  registry& operator=(const registry&) = delete;

  ~registry();

  /// Maps test names to factories. Elements are sorted by the order of their
  /// registration.
  using tests_map = unordered_flat_map<std::string_view, factory*>;

  /// Maps suite names to suites.
  using suites_map = std::map<std::string_view, tests_map>;

  static suites_map suites();

  /// Adds a new test factory to the suite `suite_name`.
  template <class TestImpl>
  static ptrdiff_t add(std::string_view suite_name,
                       std::string_view description, block_type type) {
    struct impl : factory {
      using factory::factory;
      std::unique_ptr<runnable> make(context_ptr state) override {
        return std::make_unique<TestImpl>(std::move(state), this->description_,
                                          this->type_);
      }
    };
    return instance().add(new impl(suite_name, description, type));
  }

  /// Adds a new test factory to the "anonymous" suite named `$`.
  template <class TestImpl>
  static ptrdiff_t add(unit_t, std::string_view description, block_type type) {
    return add<TestImpl>("$", description, type);
  }

  /// Adds a new callback that needs to run before the first test runs.
  static ptrdiff_t add_init_callback(void_function callback);

  /// Runs all registered init callbacks.
  static void run_init_callbacks();

private:
  ptrdiff_t add(factory* new_factory);

  ptrdiff_t add(void_function new_callback);

  static registry& instance();

  /// The head of the linked list of factories.
  factory* head_ = nullptr;

  /// The tail of the linked list of factories.
  factory* tail_ = nullptr;

  /// The head of the linked list (stack) of init callbacks.
  init_callback* init_stack_ = nullptr;
};

} // namespace caf::test

#define TEST_INIT()                                                            \
  struct CAF_PP_UNIFYN(test_init_) {                                           \
    static void do_init();                                                     \
    static ptrdiff_t register_id;                                              \
  };                                                                           \
  ptrdiff_t CAF_PP_UNIFYN(test_init_)::register_id                             \
    = caf::test::registry::add_init_callback(                                  \
      CAF_PP_UNIFYN(test_init_)::do_init);                                     \
  void CAF_PP_UNIFYN(test_init_)::do_init()
