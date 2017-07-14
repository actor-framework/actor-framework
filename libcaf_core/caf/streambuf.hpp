/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#ifndef CAF_STREAMBUF_HPP
#define CAF_STREAMBUF_HPP

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <streambuf>
#include <type_traits>
#include <vector>

#include "caf/config.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf {

/// The base class for all stream buffer implementations.
template <class CharT = char, class Traits = std::char_traits<CharT>>
class stream_buffer : public std::basic_streambuf<CharT, Traits> {
protected:
  /// The standard only defines pbump(int), which can overflow on 64-bit
  /// architectures. All stream buffer implementations should therefore use
  /// these function instead. For a detailed discussion, see:
  /// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=47921
  template <class T = int>
  typename std::enable_if<sizeof(T) == 4>::type
  safe_pbump(std::streamsize n) {
    while (n > std::numeric_limits<int>::max()) {
      this->pbump(std::numeric_limits<int>::max());
      n -= std::numeric_limits<int>::max();
    }
    this->pbump(static_cast<int>(n));
  }

  template <class T = int>
  typename std::enable_if<sizeof(T) == 8>::type
  safe_pbump(std::streamsize n) {
    this->pbump(static_cast<int>(n));
  }

  // As above, but for the get area.
  template <class T = int>
  typename std::enable_if<sizeof(T) == 4>::type
  safe_gbump(std::streamsize n) {
    while (n > std::numeric_limits<int>::max()) {
      this->gbump(std::numeric_limits<int>::max());
      n -= std::numeric_limits<int>::max();
    }
    this->gbump(static_cast<int>(n));
  }

  template <class T = int>
  typename std::enable_if<sizeof(T) == 8>::type
  safe_gbump(std::streamsize n) {
    this->gbump(static_cast<int>(n));
  }
};

/// A streambuffer abstraction over a fixed array of bytes. This streambuffer
/// cannot overflow/underflow. Once it has reached its end, attempts to read
/// characters will return `trait_type::eof`.
template <class CharT = char, class Traits = std::char_traits<CharT>>
class arraybuf : public stream_buffer<CharT, Traits> {
public:
  using base = std::basic_streambuf<CharT, Traits>;
  using char_type = typename base::char_type;
  using traits_type = typename base::traits_type;
  using int_type = typename base::int_type;
  using pos_type = typename base::pos_type;
  using off_type = typename base::off_type;

  /// Constructs an array streambuffer from a container.
  /// @param c A contiguous container.
  /// @pre `c.data()` must point to a contiguous sequence of characters having
  ///      length `c.size()`.
  template <
    class Container,
    class = typename std::enable_if<
      detail::has_data_member<Container>::value
      && detail::has_size_member<Container>::value
    >::type
  >
  arraybuf(Container& c)
    : arraybuf(const_cast<char_type*>(c.data()), c.size()) {
    // nop
  }

  /// Constructs an array streambuffer from a raw character sequence.
  /// @param data A pointer to the first character.
  /// @param size The length of the character sequence.
  arraybuf(char_type* data, size_t size) {
    setbuf(data, static_cast<std::streamsize>(size));
  }

  // There exists a bug in libstdc++ version < 5: the implementation does not
  // provide  the necessary move constructors, so we have to roll our own :-/.
  // See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=54316 for details.
  // TODO: remove after having raised the minimum GCC version to 5.
  arraybuf(arraybuf&& other) {
    this->setg(other.eback(), other.gptr(), other.egptr());
    this->setp(other.pptr(), other.epptr());
    other.setg(nullptr, nullptr, nullptr);
    other.setp(nullptr, nullptr);
  }

  // TODO: remove after having raised the minimum GCC version to 5.
  arraybuf& operator=(arraybuf&& other) {
    this->setg(other.eback(), other.gptr(), other.egptr());
    this->setp(other.pptr(), other.epptr());
    other.setg(nullptr, nullptr, nullptr);
    other.setp(nullptr, nullptr);
    return *this;
  }

protected:
  // -- positioning ----------------------------------------------------------

  std::basic_streambuf<char_type, Traits>*
  setbuf(char_type* s, std::streamsize n) override {
    this->setg(s, s, s + n);
    this->setp(s, s + n);
    return this;
  }

  pos_type seekpos(pos_type pos,
                   std::ios_base::openmode which
                     = std::ios_base::in | std::ios_base::out) override {
    auto get = (which & std::ios_base::in) == std::ios_base::in;
    auto put = (which & std::ios_base::out) == std::ios_base::out;
    if (!(get || put))
      return pos_type(off_type(-1)); // nothing to do
    if (get)
      this->setg(this->eback(), this->eback() + pos, this->egptr());
    if (put) {
      this->setp(this->pbase(), this->epptr());
      this->safe_pbump(pos);
    }
    return pos;
  }

  pos_type seekoff(off_type off,
                   std::ios_base::seekdir dir,
                   std::ios_base::openmode which) override {
    auto new_off = pos_type(off_type(-1));
    auto get = (which & std::ios_base::in) == std::ios_base::in;
    auto put = (which & std::ios_base::out) == std::ios_base::out;
    if (!(get || put))
      return new_off; // nothing to do
    if (get) {
      switch (dir) {
        default:
          return pos_type(off_type(-1));
        case std::ios_base::beg:
          new_off = 0;
          break;
        case std::ios_base::cur:
          new_off = this->gptr() - this->eback();
          break;
        case std::ios_base::end:
          new_off = this->egptr() - this->eback();
          break;
      }
      new_off += off;
      this->setg(this->eback(), this->eback() + new_off, this->egptr());
    }
    if (put) {
      switch (dir) {
        default:
          return pos_type(off_type(-1));
        case std::ios_base::beg:
          new_off = 0;
          break;
        case std::ios_base::cur:
          new_off = this->pptr() - this->pbase();
          break;
        case std::ios_base::end:
          new_off = this->egptr() - this->pbase();
          break;
      }
      new_off += off;
      this->setp(this->pbase(), this->epptr());
      this->safe_pbump(new_off);
    }
    return new_off;
  }

  // -- put area -------------------------------------------------------------

  std::streamsize xsputn(const char_type* s, std::streamsize n) override {
    auto available = this->epptr() - this->pptr();
    auto actual = std::min(n, static_cast<std::streamsize>(available));
    std::memcpy(this->pptr(), s,
                static_cast<size_t>(actual) * sizeof(char_type));
    this->safe_pbump(actual);
    return actual;
  }

  // -- get area -------------------------------------------------------------

  std::streamsize xsgetn(char_type* s, std::streamsize n) override {
    auto available = this->egptr() - this->gptr();
    auto actual = std::min(n, static_cast<std::streamsize>(available));
    std::memcpy(s, this->gptr(),
                static_cast<size_t>(actual) * sizeof(char_type));
    this->safe_gbump(actual);
    return actual;
  }
};

/// A streambuffer abstraction over a contiguous container. It supports
/// reading in the same style as `arraybuf`, but is unbounded for output.
template <class Container>
class containerbuf
  : public stream_buffer<
      typename Container::value_type,
      std::char_traits<typename Container::value_type>
    > {
public:
  using base = std::basic_streambuf<
      typename Container::value_type,
      std::char_traits<typename Container::value_type>
    >;
  using char_type = typename base::char_type;
  using traits_type = typename base::traits_type;
  using int_type = typename base::int_type;
  using pos_type = typename base::pos_type;
  using off_type = typename base::off_type;

  /// Constructs a container streambuf.
  /// @param c A contiguous container.
  template <
    class C = Container,
    class = typename std::enable_if<
      detail::has_data_member<C>::value && detail::has_size_member<C>::value
    >::type
  >
  containerbuf(Container& c) : container_(c) {
    this->setg(const_cast<char_type*>(c.data()),
               const_cast<char_type*>(c.data()),
               const_cast<char_type*>(c.data()) + c.size());
  }

  // See note in arraybuf(arraybuf&&).
  // TODO: remove after having raised the minimum GCC version to 5.
  containerbuf(containerbuf&& other)
    : container_(other.container_) {
    this->setg(other.eback(), other.gptr(), other.egptr());
    other.setg(nullptr, nullptr, nullptr);
  }

  // TODO: remove after having raised the minimum GCC version to 5.
  containerbuf& operator=(containerbuf&& other) {
    this->setg(other.eback(), other.gptr(), other.egptr());
    other.setg(nullptr, nullptr, nullptr);
    return *this;
  }

  // Hides base-class implementation to simplify single-character lookup.
  int_type sgetc() {
    if (this->gptr() == this->egptr())
      return traits_type::eof();
    return traits_type::to_int_type(*this->gptr());
  }

  // Hides base-class implementation to simplify single-character insert.
  int_type sputc(char_type c) {
    container_.push_back(c);
    return c;
  }

protected:
  // We can't get obtain more characters on underflow, so we only optimize
  // multi-character sequential reads.
  std::streamsize xsgetn(char_type* s, std::streamsize n) override {
    auto available = this->egptr() - this->gptr();
    auto actual = std::min(n, static_cast<std::streamsize>(available));
    std::memcpy(s, this->gptr(),
                static_cast<size_t>(actual) * sizeof(char_type));
    this->safe_gbump(actual);
    return actual;
  }

  // Should never get called, because there is always space in the buffer.
  // (But just in case, it does the same thing as sputc.)
  int_type overflow(int_type c = traits_type::eof()) final {
    if (!traits_type::eq_int_type(c, traits_type::eof()))
      container_.push_back(traits_type::to_char_type(c));
    return c;
  }

  std::streamsize xsputn(const char_type* s, std::streamsize n) override {
    // TODO: Do a performance analysis whether the current implementation based
    // on copy_n is faster than these two statements:
    // (1) container_.resize(container_.size() + n);
    // (2) std::memcpy(this->pptr(), s, n * sizeof(char_type));
    std::copy_n(s, n, std::back_inserter(container_));
    return n;
  }

private:
  Container& container_;
};

using charbuf = arraybuf<char>;
using vectorbuf = containerbuf<std::vector<char>>;

} // namespace caf

#endif // CAF_STREAMBUF_HPP
