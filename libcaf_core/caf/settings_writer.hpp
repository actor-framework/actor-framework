/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include "caf/config_value.hpp"
#include "caf/serializer.hpp"
#include "caf/settings.hpp"

#include <stack>
#include <vector>

namespace caf {

/// Writes objects into @ref settings.
class settings_writer final : public serializer {
public:
  // -- member types------------------------------------------------------------

  using super = serializer;

  struct present_field {
    settings* parent;
    string_view name;
    string_view type;
  };

  struct absent_field {};

  using value_type
    = variant<settings*, absent_field, present_field, config_value::list*>;

  using stack_type = std::stack<value_type, std::vector<value_type>>;

  // -- constructors, destructors, and assignment operators --------------------

  settings_writer(settings* destination, actor_system& sys)
    : super(sys), root_(destination) {
    st_.push(destination);
    has_human_readable_format_ = true;
  }

  settings_writer(settings* destination, execution_unit* ctx)
    : super(ctx), root_(destination) {
    has_human_readable_format_ = true;
  }

  explicit settings_writer(settings* destination)
    : settings_writer(destination, nullptr) {
    // nop
  }

  ~settings_writer() override;

  // -- interface functions ----------------------------------------------------

  bool inject_next_object_type(type_id_t type) override;

  bool begin_object(string_view name) override;

  bool end_object() override;

  bool begin_field(string_view) override;

  bool begin_field(string_view name, bool is_present) override;

  bool begin_field(string_view name, span<const type_id_t> types,
                   size_t index) override;

  bool begin_field(string_view name, bool is_present,
                   span<const type_id_t> types, size_t index) override;

  bool end_field() override;

  bool begin_tuple(size_t size) override;

  bool end_tuple() override;

  bool begin_key_value_pair() override;

  bool end_key_value_pair() override;

  bool begin_sequence(size_t size) override;

  bool end_sequence() override;

  bool begin_associative_array(size_t size) override;

  bool end_associative_array() override;

  bool value(bool x) override;

  bool value(int8_t x) override;

  bool value(uint8_t x) override;

  bool value(int16_t x) override;

  bool value(uint16_t x) override;

  bool value(int32_t x) override;

  bool value(uint32_t x) override;

  bool value(int64_t x) override;

  bool value(uint64_t x) override;

  bool value(float x) override;

  bool value(double x) override;

  bool value(long double x) override;

  bool value(string_view x) override;

  bool value(const std::u16string& x) override;

  bool value(const std::u32string& x) override;

  bool value(span<const byte> x) override;

private:
  bool push(config_value&& x);

  stack_type st_;
  string_view type_hint_;
  settings* root_;
};

} // namespace caf
