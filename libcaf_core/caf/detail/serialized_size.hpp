// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

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

  bool begin_object(type_id_t, std::string_view) override;

  bool end_object() override;

  bool begin_field(std::string_view) override;

  bool begin_field(std::string_view, bool is_present) override;

  bool begin_field(std::string_view, span<const type_id_t> types,
                   size_t index) override;

  bool begin_field(std::string_view, bool is_present,
                   span<const type_id_t> types, size_t index) override;

  bool end_field() override;

  bool begin_tuple(size_t size) override;

  bool end_tuple() override;

  bool begin_sequence(size_t size) override;

  bool end_sequence() override;

  bool value(std::byte x) override;

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

  bool value(std::string_view x) override;

  bool value(const std::u16string& x) override;

  bool value(const std::u32string& x) override;

  bool value(span<const std::byte> x) override;

  using super::list;

  bool list(const std::vector<bool>& xs) override;
};

template <class T>
size_t serialized_size(const T& x) {
  serialized_size_inspector f;
  auto unused = f.apply(x);
  static_cast<void>(unused); // Always true.
  return f.result;
}

template <class T>
size_t serialized_size(actor_system& sys, const T& x) {
  serialized_size_inspector f{sys};
  auto unused = f.apply(x);
  static_cast<void>(unused); // Always true.
  return f.result;
}

} // namespace caf::detail
