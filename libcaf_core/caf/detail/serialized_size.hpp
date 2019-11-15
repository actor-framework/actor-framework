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

#include "caf/serializer.hpp"

namespace caf::detail {

class serialized_size_inspector final : public serializer {
public:
  using super = serializer;

  using super::super;

  size_t result() const noexcept {
    return result_;
  }

  error begin_object(uint16_t& nr, std::string& name) override;

  error end_object() override;

  error begin_sequence(size_t& list_size) override;

  error end_sequence() override;

  error apply_raw(size_t num_bytes, void* data) override;

protected:
  error apply_impl(int8_t& x) override;

  error apply_impl(uint8_t& x) override;

  error apply_impl(int16_t& x) override;

  error apply_impl(uint16_t& x) override;

  error apply_impl(int32_t& x) override;

  error apply_impl(uint32_t& x) override;

  error apply_impl(int64_t& x) override;

  error apply_impl(uint64_t& x) override;

  error apply_impl(float& x) override;

  error apply_impl(double& x) override;

  error apply_impl(long double& x) override;

  error apply_impl(std::string& x) override;

  error apply_impl(std::u16string& x) override;

  error apply_impl(std::u32string& x) override;

private:
  size_t result_ = 0;
};

template <class T>
size_t serialized_size(actor_system& sys, const T& x) {
  serialized_size_inspector f{sys};
  f(const_cast<T&>(x));
  return f.result();
}

} // namespace caf
