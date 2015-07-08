/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_BINARY_SERIALIZER_HPP
#define CAF_BINARY_SERIALIZER_HPP

#include <utility>
#include <sstream>
#include <iomanip>
#include <functional>
#include <type_traits>

#include "caf/fwd.hpp"
#include "caf/serializer.hpp"
#include "caf/primitive_variant.hpp"

#include "caf/detail/ieee_754.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf {

/// Implements the serializer interface with a binary serialization protocol.
class binary_serializer : public serializer {
public:
  /// Creates a binary serializer writing to given iterator position.
  template <class OutIter>
  explicit binary_serializer(OutIter iter, actor_namespace* ns = nullptr)
      : serializer(ns) {
    reset(iter);
  }

  using write_fun = std::function<void(const char*, size_t)>;

  void begin_object(const uniform_type_info* uti) override;

  void end_object() override;

  void begin_sequence(size_t list_size) override;

  void end_sequence() override;

  void write_value(const primitive_variant& value) override;

  void write_raw(size_t num_bytes, const void* data) override;

  template <class OutIter>
  void reset(OutIter iter) {
    struct fun {
      fun(OutIter pos) : pos_(pos) {
        // nop
      }
      void operator()(const char* first, size_t num_bytes) {
        pos_ = std::copy(first, first + num_bytes, pos_);
      }
      OutIter pos_;
    };
    out_ = fun{iter};
  }

private:
  write_fun out_;
};

} // namespace caf

#endif // CAF_BINARY_SERIALIZER_HPP
