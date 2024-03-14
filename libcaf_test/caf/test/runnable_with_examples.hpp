// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/test/runnable.hpp"

#include "caf/detail/test_export.hpp"

namespace caf::test {

class CAF_TEST_EXPORT runnable_with_examples : public runnable {
public:
  using super = runnable;

  class CAF_TEST_EXPORT examples_setter {
  public:
    using examples_t = std::vector<std::map<std::string, std::string>>;

    explicit examples_setter(examples_t* examples) : examples_(examples) {
      // nop
    }

    examples_setter(const examples_setter&) = default;

    examples_setter& operator=(const examples_setter&) = default;

    examples_setter& operator=(std::string_view str);

  private:
    examples_t* examples_;
  };

  using super::super;

  auto make_examples_setter() {
    if (ctx_->example_parameters.empty())
      return examples_setter{&ctx_->example_parameters};
    else
      return examples_setter{nullptr};
  }

protected:
  void run() override;
};

} // namespace caf::test
