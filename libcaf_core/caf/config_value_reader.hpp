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

#include "caf/deserializer.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/dictionary.hpp"
#include "caf/fwd.hpp"

#include <memory>
#include <stack>
#include <vector>

namespace caf {

/// Extracts objects from a @ref config_value.
class CAF_CORE_EXPORT config_value_reader final : public deserializer {
public:
  // -- member types------------------------------------------------------------

  using super = deserializer;

  using key_ptr = const std::string*;

  struct absent_field {};

  struct sequence {
    using list_pointer = const std::vector<config_value>*;
    size_t index;
    list_pointer ls;
    explicit sequence(list_pointer ls) : index(0), ls(ls) {
      // nop
    }
    bool at_end() const noexcept;
    const config_value& current();
    void advance() {
      ++index;
    }
  };

  struct associative_array {
    settings::const_iterator pos;
    settings::const_iterator end;
    bool at_end() const noexcept;
    const std::pair<const std::string, config_value>& current();
  };

  using value_type = variant<const settings*, const config_value*, key_ptr,
                             absent_field, sequence, associative_array>;

  using stack_type = std::stack<value_type, std::vector<value_type>>;

  // -- constructors, destructors, and assignment operators --------------------

  config_value_reader(const config_value* input, actor_system& sys)
    : super(sys) {
    st_.push(input);
    has_human_readable_format_ = true;
  }

  config_value_reader(const config_value* input, execution_unit* ctx)
    : super(ctx) {
    st_.push(input);
    has_human_readable_format_ = true;
  }
  explicit config_value_reader(const config_value* input)
    : config_value_reader(input, nullptr) {
    // nop
  }

  ~config_value_reader() override;

  config_value_reader(const config_value_reader&) = delete;

  config_value_reader& operator=(const config_value_reader&) = delete;

  // -- stack access -----------------------------------------------------------

  value_type& top() {
    return st_.top();
  }

  void pop() {
    return st_.pop();
  }

  // -- interface functions ----------------------------------------------------

  bool fetch_next_object_type(type_id_t& type) override;

  bool begin_object(type_id_t type, string_view name) override;

  bool end_object() override;

  bool begin_field(string_view) override;

  bool begin_field(string_view name, bool& is_present) override;

  bool begin_field(string_view name, span<const type_id_t> types,
                   size_t& index) override;

  bool begin_field(string_view name, bool& is_present,
                   span<const type_id_t> types, size_t& index) override;

  bool end_field() override;

  bool begin_tuple(size_t size) override;

  bool end_tuple() override;

  bool begin_key_value_pair() override;

  bool end_key_value_pair() override;

  bool begin_sequence(size_t& size) override;

  bool end_sequence() override;

  bool begin_associative_array(size_t& size) override;

  bool end_associative_array() override;

  bool value(byte& x) override;

  bool value(bool& x) override;

  bool value(int8_t& x) override;

  bool value(uint8_t& x) override;

  bool value(int16_t& x) override;

  bool value(uint16_t& x) override;

  bool value(int32_t& x) override;

  bool value(uint32_t& x) override;

  bool value(int64_t& x) override;

  bool value(uint64_t& x) override;

  bool value(float& x) override;

  bool value(double& x) override;

  bool value(long double& x) override;

  bool value(std::string& x) override;

  bool value(std::u16string& x) override;

  bool value(std::u32string& x) override;

  bool value(span<byte> x) override;

private:
  bool fetch_object_type(const settings* obj, type_id_t& type);

  stack_type st_;

  // Stores on-the-fly converted values.
  std::vector<std::unique_ptr<config_value>> scratch_space_;
};

} // namespace caf
