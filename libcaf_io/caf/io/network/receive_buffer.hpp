/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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

#include <cstddef>
#include <cstring>
#include <memory>

#include "caf/allowed_unsafe_message_type.hpp"

namespace caf {
namespace io {
namespace network {

/// A container that does not call constructors and destructors for its values.
class receive_buffer {
public:
  using value_type = char;
  using size_type = size_t;
  using difference_type = std::ptrdiff_t;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = std::pointer_traits<pointer>::rebind<const value_type>;
  using iterator = pointer;
  using const_iterator = const_pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using buffer_ptr = std::unique_ptr<value_type[],
                                     std::default_delete<value_type[]>>;

  /// Create an empty container.
  receive_buffer() noexcept;

  /// Create an empty container of size `count`. Data in the storage is not
  /// initialized.
  receive_buffer(size_t count);

  /// Move constructor.
  receive_buffer(receive_buffer&& other) noexcept;

  /// Copy constructor.
  receive_buffer(const receive_buffer& other);

  /// Move assignment operator.
  receive_buffer& operator=(receive_buffer&& other) noexcept;

  /// Copy assignment operator.
  receive_buffer& operator=(const receive_buffer& other);

  /// Returns a pointer to the underlying buffer.
  inline pointer data() noexcept {
    return buffer_.get();
  }

  /// Returns a const pointer to the data.
  inline const_pointer data() const noexcept {
    return buffer_.get();
  }

  /// Returns the number of stored elements.
  inline size_type size() const noexcept {
    return size_;
  }

  /// Returns the number of elements that the container has allocated space for.
  inline size_type capacity() const noexcept {
    return capacity_;
  }

  /// Returns the maximum possible number of elements the container
  /// could theoretically hold.
  inline size_type max_size() const noexcept {
    return std::numeric_limits<size_t>::max();
  }

  /// Resize the container to `new_size`. While this may increase its storage,
  /// no storage will be released.
  void resize(size_type new_size);

  /// Set the size of the storage to `new_size`. If `new_size` is smaller than
  /// the current capacity nothing happens. If `new_size` is larger than the
  /// current capacity all iterators are invalidated.
  void reserve(size_type new_size);

  /// Shrink the container to its current size.
  void shrink_to_fit();

  /// Check if the container is empty.
  inline bool empty() const noexcept {
    return size_ == 0;
  }

  /// Clears the content of the container and releases the allocated storage.
  void clear();

  /// Swap contents with `other` receive buffer.
  void swap(receive_buffer& other) noexcept;

  /// Returns an iterator to the beginning.
  inline iterator begin() noexcept {
    return buffer_.get();
  }

  /// Returns an iterator to the end.
  inline iterator end() noexcept {
    return buffer_.get() + size_;
  }

  /// Returns an iterator to the beginning.
  inline const_iterator begin() const noexcept {
    return buffer_.get();
  }

  /// Returns an iterator to the end.
  inline const_iterator end() const noexcept {
    return buffer_.get() + size_;
  }

  /// Returns an iterator to the beginning.
  inline const_iterator cbegin() const noexcept {
    return buffer_.get();
  }

  /// Returns an iterator to the end.
  inline const_iterator cend() const noexcept {
    return buffer_.get() + size_;
  }

  /// Returns jan iterator to the reverse beginning.
  inline reverse_iterator rbegin() noexcept {
    return reverse_iterator{buffer_.get() + size_};
  }

  /// Returns an iterator to the reverse end of the data.
  inline reverse_iterator rend() noexcept {
    return reverse_iterator{buffer_.get()};
  }

  /// Returns an iterator to the reverse beginning.
  inline const_reverse_iterator rbegin() const noexcept {
    return const_reverse_iterator{buffer_.get() + size_};
  }

  /// Returns an iterator to the reverse end of the data.
  inline const_reverse_iterator rend() const noexcept {
    return const_reverse_iterator{buffer_.get()};
  }

  /// Returns an iterator to the reverse beginning.
  inline const_reverse_iterator crbegin() const noexcept {
    return const_reverse_iterator{buffer_.get() + size_};
  }

  /// Returns an iterator to the reverse end of the data.
  inline const_reverse_iterator crend() const noexcept {
    return const_reverse_iterator{buffer_.get()};
  }

  /// Insert `value` before `pos`.
  iterator insert(iterator pos, value_type value);

  /// Insert `value` before `pos`.
  template <class InputIterator>
  iterator insert(iterator pos, InputIterator first, InputIterator last) {
    auto n = std::distance(first, last);
    if (n == 0)
      return pos;
    auto offset = static_cast<size_t>(std::distance(begin(), pos));
    auto old_size = size_;
    resize(old_size + static_cast<size_t>(n));
    pos = begin() + offset;
    if (offset != old_size) {
      memmove(pos + n, pos, old_size - offset);
    }
    return std::copy(first, last, pos);
  }

  /// Append `value`.
  void push_back(value_type value);

private:
  // Increse the buffer capacity, maintaining its data. May invalidate iterators.
  void increase_by(size_t bytes);

  // Reduce the buffer capacity, maintaining its data. May invalidate iterators.
  void shrink_by(size_t bytes);

  buffer_ptr buffer_;
  size_type capacity_;
  size_type size_;
};

} // namepsace network
} // namespace io
} // namespace caf

