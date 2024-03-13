// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/test/context.hpp"
#include "caf/test/scenario.hpp"

#include "caf/detail/test_export.hpp"

namespace caf::test {

class CAF_TEST_EXPORT outline_setter : public runnable {
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

class CAF_TEST_EXPORT outline : public block {
public:
  using block::block;

  block_type type() const noexcept override;

  given* get_given(int id, std::string_view description,
                   const detail::source_location& loc) override;

  and_given* get_and_given(int id, std::string_view description,
                           const detail::source_location& loc) override;

  when* get_when(int id, std::string_view description,
                 const detail::source_location& loc) override;

  and_when* get_and_when(int id, std::string_view description,
                         const detail::source_location& loc) override;

  scope commit();
};

} // namespace caf::test

#define OUTLINE(description)                                                   \
  struct CAF_PP_UNIFYN(outline_)                                               \
    : caf::test::outline_setter, caf_test_case_auto_fixture {                  \
    using super = caf::test::outline_setter;                                   \
    using super::super;                                                        \
    void do_run() override;                                                    \
    static ptrdiff_t register_id;                                              \
  };                                                                           \
  ptrdiff_t CAF_PP_UNIFYN(outline_)::register_id                               \
    = caf::test::registry::add<CAF_PP_UNIFYN(outline_)>(                       \
      caf_test_suite_name, description, caf::test::block_type::outline);       \
  void CAF_PP_UNIFYN(outline_)::do_run()

#define EXAMPLES this->make_examples_setter()
