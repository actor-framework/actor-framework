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
#include <iterator>
#include <functional>
#include <type_traits>

#include "caf/fwd.hpp"
#include "caf/serializer.hpp"
#include "caf/primitive_variant.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {

/// Implements the serializer interface with a binary serialization protocol.
class binary_serializer : public serializer {
public:
  /// Creates a binary serializer writing to given iterator position.
  template <class C,
            class Out,
            class E =
              typename std::enable_if<
                std::is_same<
                  typename std::iterator_traits<Out>::iterator_category,
                  std::output_iterator_tag
                >::value
                || std::is_same<
                    typename std::iterator_traits<Out>::iterator_category,
                    std::random_access_iterator_tag
                  >::value
              >::type>
  binary_serializer(C&& ctx, Out it) : serializer(std::forward<C>(ctx)) {
    reset_iter(it);
  }

  template <class C,
            class T,
            class E =
              typename std::enable_if<
                detail::has_char_insert<T>::value
              >::type>
  binary_serializer(C&& ctx, T& out) : serializer(std::forward<C>(ctx)) {
    reset_container(out);
  }

  using write_fun = std::function<void(const char*, size_t)>;

  void begin_object(uint16_t& typenr, std::string& name) override;

  void end_object() override;

  void begin_sequence(size_t& list_size) override;

  void end_sequence() override;

  void apply_raw(size_t num_bytes, void* data) override;

  void apply_builtin(builtin in_out_type, void* in_out) override;

  template <class OutIter>
  void reset_iter(OutIter iter) {
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

  template <class T>
  void reset_container(T& x) {
    struct fun {
      fun(T& container) : x_(container) {
        // nop
      }
      void operator()(const char* first, size_t num_bytes) {
        auto last = first + num_bytes;
        x_.insert(x_.end(), first, last);
      }
      T& x_;
    };
    out_ = fun{x};
  }

private:
  write_fun out_;
};

} // namespace caf

#endif // CAF_BINARY_SERIALIZER_HPP
