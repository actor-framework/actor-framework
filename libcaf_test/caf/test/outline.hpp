// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/test/context.hpp"
#include "caf/test/runnable_with_examples.hpp"
#include "caf/test/scenario.hpp"

#include "caf/detail/test_export.hpp"

namespace caf::test {

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
    : caf::test::runnable_with_examples, caf_test_case_auto_fixture {          \
    using super = caf::test::runnable_with_examples;                           \
    using super::super;                                                        \
    void do_run() override;                                                    \
    static ptrdiff_t register_id;                                              \
  };                                                                           \
  ptrdiff_t CAF_PP_UNIFYN(outline_)::register_id                               \
    = caf::test::registry::add<CAF_PP_UNIFYN(outline_)>(                       \
      caf_test_suite_name, description, caf::test::block_type::outline);       \
  void CAF_PP_UNIFYN(outline_)::do_run()

#define EXAMPLES this->make_examples_setter()
