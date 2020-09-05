/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
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

#include "caf/detail/core_export.hpp"
#include "caf/error.hpp"
#include "caf/serializer.hpp"

namespace caf::detail {

class CAF_CORE_EXPORT serialized_size_inspector final : public serializer {
public:
  using super = serializer;

  using super::super;

  size_t result = 0;

  bool inject_next_object_type(type_id_t type) override;

  bool begin_object(string_view) override;

  bool end_object() override;

  bool begin_field(string_view) override;

  bool begin_field(string_view, bool is_present) override;

  bool begin_field(string_view, span<const type_id_t> types,
                   size_t index) override;

  bool begin_field(string_view, bool is_present, span<const type_id_t> types,
                   size_t index) override;

  bool end_field() override;

  bool begin_tuple(size_t size) override;

  bool end_tuple() override;

  bool begin_sequence(size_t size) override;

  bool end_sequence() override;

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

  bool value(const std::vector<bool>& xs) override;
};

template <class T>
size_t serialized_size(const T& x) {
  serialized_size_inspector f;
  auto inspection_res = inspect_object(f, detail::as_mutable_ref(x));
  static_cast<void>(inspection_res); // Always true.
  return f.result;
}

template <class T>
size_t serialized_size(actor_system& sys, const T& x) {
  serialized_size_inspector f{sys};
  auto inspection_res = inspect_object(f, detail::as_mutable_ref(x));
  static_cast<void>(inspection_res); // Always true.
  return f.result;
}

} // namespace caf::detail
