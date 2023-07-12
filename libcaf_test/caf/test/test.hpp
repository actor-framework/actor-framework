// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/pp.hpp"
#include "caf/detail/test_export.hpp"
#include "caf/test/block.hpp"
#include "caf/test/context.hpp"
#include "caf/test/factory.hpp"
#include "caf/test/registry.hpp"
#include "caf/test/runnable.hpp"
#include "caf/test/scope.hpp"
#include "caf/test/section.hpp"
#include "caf/test/suite.hpp"

namespace caf::test {

/// Represents a `TEST` block.
class CAF_TEST_EXPORT test : public block {
public:
  using block::block;

  block_type type() const noexcept override;

  section* get_section(int id, std::string_view description,
                       const detail::source_location& loc) override;

  scope commit();
};

} // namespace caf::test

#define SECTION(description)                                                   \
  for (auto CAF_PP_UNIFYN(scope_) = this->current_block()                      \
                                      .get_section(__COUNTER__, description)   \
                                      ->commit();                              \
       CAF_PP_UNIFYN(scope_); CAF_PP_UNIFYN(scope_).leave())

#define TEST(description)                                                      \
  struct CAF_PP_UNIFYN(test_)                                                  \
    : caf::test::runnable, caf_test_case_auto_fixture {                        \
    using super = caf::test::runnable;                                         \
    using super::super;                                                        \
    void do_run() override;                                                    \
    static ptrdiff_t register_id;                                              \
  };                                                                           \
  ptrdiff_t CAF_PP_UNIFYN(test_)::register_id                                  \
    = caf::test::registry::add<CAF_PP_UNIFYN(test_)>(                          \
      caf_test_suite_name, description, caf::test::block_type::test);          \
  void CAF_PP_UNIFYN(test_)::do_run()
