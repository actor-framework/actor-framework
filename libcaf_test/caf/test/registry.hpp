// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/test_export.hpp"
#include "caf/test/block_type.hpp"
#include "caf/test/factory.hpp"
#include "caf/test/fwd.hpp"
#include "caf/unordered_flat_map.hpp"

#include <cstddef>
#include <map>
#include <string_view>

namespace caf::test {

/// A registry for our factories.
class CAF_TEST_EXPORT registry {
public:
  /// Maps test names to factories. Elements are sorted by the order of their
  /// registration.
  using tests_map = unordered_flat_map<std::string_view, factory*>;

  /// Maps suite names to suites.
  using suites_map = std::map<std::string_view, tests_map>;

  static suites_map suites();

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
    auto ptr = std::make_unique<impl>(suite_name, description, type);
    auto result = reinterpret_cast<ptrdiff_t>(ptr.get());
    if (head_ == nullptr) {
      head_ = std::move(ptr);
      tail_ = head_.get();
    } else {
      tail_->next_ = std::move(ptr);
      tail_ = tail_->next_.get();
    }
    return result;
  }

private:
  static std::unique_ptr<factory> head_;
  static factory* tail_;
};

} // namespace caf::test
