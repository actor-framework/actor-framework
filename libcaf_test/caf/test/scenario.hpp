// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/test/and_given.hpp"
#include "caf/test/and_then.hpp"
#include "caf/test/and_when.hpp"
#include "caf/test/block.hpp"
#include "caf/test/context.hpp"
#include "caf/test/factory.hpp"
#include "caf/test/given.hpp"
#include "caf/test/registry.hpp"
#include "caf/test/runnable.hpp"
#include "caf/test/scope.hpp"
#include "caf/test/suite.hpp"
#include "caf/test/then.hpp"
#include "caf/test/when.hpp"

#include "caf/detail/pp.hpp"
#include "caf/detail/test_export.hpp"

namespace caf::test {

class CAF_TEST_EXPORT scenario : public block {
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

#define GIVEN(description)                                                     \
  for (auto given_scope                                                        \
       = this->current_block().get_given(__COUNTER__, description)->commit();  \
       given_scope; given_scope.leave())

#define AND_GIVEN(description)                                                 \
  static_cast<void>(0);                                                        \
  for (auto and_given_scope = this->current_block()                            \
                                .get_and_given(__COUNTER__, description)       \
                                ->commit();                                    \
       and_given_scope.leave())

#define WHEN(description)                                                      \
  for (auto when_scope                                                         \
       = this->current_block().get_when(__COUNTER__, description)->commit();   \
       when_scope; when_scope.leave())

#define AND_WHEN(description)                                                  \
  static_cast<void>(0);                                                        \
  for (auto and_when_scope = this->current_block()                             \
                               .get_and_when(__COUNTER__, description)         \
                               ->commit();                                     \
       and_when_scope; and_when_scope.leave())

#define THEN(description)                                                      \
  for (auto then_scope                                                         \
       = this->current_block().get_then(__COUNTER__, description)->commit();   \
       then_scope; then_scope.leave())

#define AND_THEN(description)                                                  \
  static_cast<void>(0);                                                        \
  for (auto and_then_scope = this->current_block()                             \
                               .get_and_then(__COUNTER__, description)         \
                               ->commit();                                     \
       and_then_scope; and_then_scope.leave())

#define SCENARIO(description)                                                  \
  struct CAF_PP_UNIFYN(scenario_)                                              \
    : caf::test::runnable, caf_test_case_auto_fixture {                        \
    using super = caf::test::runnable;                                         \
    using super::super;                                                        \
    void do_run() override;                                                    \
    static ptrdiff_t register_id;                                              \
  };                                                                           \
  ptrdiff_t CAF_PP_UNIFYN(scenario_)::register_id                              \
    = caf::test::registry::add<CAF_PP_UNIFYN(scenario_)>(                      \
      caf_test_suite_name, description, caf::test::block_type::scenario);      \
  void CAF_PP_UNIFYN(scenario_)::do_run()
