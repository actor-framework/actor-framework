// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/test/runnable.hpp"

#include "caf/detail/test_export.hpp"
#include "caf/raise_error.hpp"

namespace caf::test {

class CAF_TEST_EXPORT runnable_with_examples : public runnable {
public:
  using super = runnable;

  class CAF_TEST_EXPORT examples_setter {
  public:
    using example_t = std::map<std::string, std::string>;

    using examples_t = std::vector<example_t>;

    constexpr explicit examples_setter(examples_t* examples) noexcept
      : examples_(examples) {
      // nop
    }

    constexpr examples_setter(const examples_setter&) noexcept = default;

    constexpr examples_setter& operator=(const examples_setter&) noexcept
      = default;

    examples_setter& operator=(std::string_view str);

    void append(example_t what) {
      if (!examples_) {
        CAF_RAISE_ERROR("examples_setter is not initialized");
      }
      examples_->emplace_back(std::move(what));
    }

    explicit operator bool() const {
      return examples_ != nullptr;
    }

  private:
    examples_t* examples_;
  };

  using super::super;

  auto make_examples_setter() {
    if (test_context().example_parameters.empty())
      return examples_setter{&test_context().example_parameters};
    else
      return examples_setter{nullptr};
  }

protected:
  void run_next_test_branch_init() override;
};

} // namespace caf::test
