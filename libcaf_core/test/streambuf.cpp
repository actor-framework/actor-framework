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

#include "caf/config.hpp"

#define CAF_SUITE streambuf
#include "caf/test/unit_test.hpp"

#include "caf/streambuf.hpp"

using namespace caf;

CAF_TEST(signed_arraybuf) {
  auto data = std::string{"The quick brown fox jumps over the lazy dog"};
  arraybuf<char> ab{data};
  // Let's read some.
  CAF_CHECK_EQUAL(static_cast<size_t>(ab.in_avail()), data.size());
  CAF_CHECK_EQUAL(ab.sgetc(), 'T');
  std::string buf;
  buf.resize(3);
  auto got = ab.sgetn(&buf[0], 3);
  CAF_CHECK_EQUAL(got, 3);
  CAF_CHECK_EQUAL(buf, "The");
  CAF_CHECK_EQUAL(ab.sgetc(), ' ');
  // Exhaust the stream.
  buf.resize(data.size());
  got = ab.sgetn(&buf[0] + 3, static_cast<std::streamsize>(data.size() - 3));
  CAF_CHECK_EQUAL(static_cast<size_t>(got), data.size() - 3);
  CAF_CHECK_EQUAL(data, buf);
  CAF_CHECK_EQUAL(ab.in_avail(), 0);
  // No more.
  auto c = ab.sgetc();
  CAF_CHECK_EQUAL(c, charbuf::traits_type::eof());
  // Reset the stream and write into it.
  ab.pubsetbuf(&data[0], static_cast<std::streamsize>(data.size()));
  CAF_CHECK_EQUAL(static_cast<size_t>(ab.in_avail()), data.size());
  auto put = ab.sputn("One", 3);
  CAF_CHECK_EQUAL(put, 3);
  CAF_CHECK(data.compare(0, 3, "One") == 0);
}

CAF_TEST(unsigned_arraybuf) {
  std::vector<uint8_t> data = {0x0a, 0x0b, 0x0c, 0x0d};
  arraybuf<uint8_t> ab{data};
  decltype(data) buf;
  std::copy(std::istreambuf_iterator<uint8_t>{&ab},
            std::istreambuf_iterator<uint8_t>{},
            std::back_inserter(buf));
  CAF_CHECK_EQUAL(data, buf);
  // Relative positioning.
  using pos = arraybuf<uint8_t>::pos_type;
  CAF_CHECK_EQUAL(ab.pubseekoff(2, std::ios::beg, std::ios::in), pos{2});
  CAF_CHECK_EQUAL(ab.sbumpc(), static_cast<int>(0x0c));
  CAF_CHECK_EQUAL(ab.sgetc(), 0x0d);
  CAF_CHECK_EQUAL(ab.pubseekoff(0, std::ios::cur, std::ios::in), pos{3});
  CAF_CHECK_EQUAL(ab.pubseekoff(-2, std::ios::cur, std::ios::in), pos{1});
  CAF_CHECK_EQUAL(ab.sgetc(), 0x0b);
  CAF_CHECK_EQUAL(ab.pubseekoff(-4, std::ios::end, std::ios::in), pos{0});
  CAF_CHECK_EQUAL(ab.sgetc(), 0x0a);
  // Absolute positioning.
  CAF_CHECK_EQUAL(ab.pubseekpos(1, std::ios::in), pos{1});
  CAF_CHECK_EQUAL(ab.sgetc(), 0x0b);
  CAF_CHECK_EQUAL(ab.pubseekpos(3, std::ios::in), pos{3});
  CAF_CHECK_EQUAL(ab.sbumpc(), 0x0d);
  CAF_CHECK_EQUAL(ab.in_avail(), 0);
}

CAF_TEST(containerbuf) {
  std::string data{
    "Habe nun, ach! Philosophie,\n"
    "Juristerei und Medizin,\n"
    "Und leider auch Theologie\n"
    "Durchaus studiert, mit heißem Bemühn.\n"
    "Da steh ich nun, ich armer Tor!\n"
    "Und bin so klug als wie zuvor"
  };
  // Write some data.
  std::vector<char> buf;
  vectorbuf vb{buf};
  auto put = vb.sputn(data.data(), static_cast<std::streamsize>(data.size()));
  CAF_CHECK_EQUAL(static_cast<size_t>(put), data.size());
  put = vb.sputn(";", 1);
  CAF_CHECK_EQUAL(put, 1);
  std::string target;
  std::copy(buf.begin(), buf.end(), std::back_inserter(target));
  CAF_CHECK_EQUAL(data + ';', target);
  // Check "overflow" on a new stream.
  buf.clear();
  vectorbuf vb2{buf};
  auto chr = vb2.sputc('x');
  CAF_CHECK_EQUAL(chr, 'x');
  // Let's read some data into a stream.
  buf.clear();
  containerbuf<std::string> scb{data};
  std::copy(std::istreambuf_iterator<char>{&scb},
            std::istreambuf_iterator<char>{},
            std::back_inserter(buf));
  CAF_CHECK_EQUAL(buf.size(), data.size());
  CAF_CHECK(std::equal(buf.begin(), buf.end(), data.begin() /*, data.end() */));
  // We're done, nothing to see here, please move along.
  CAF_CHECK_EQUAL(scb.sgetc(), containerbuf<std::string>::traits_type::eof());
  // Let's read again, but now in one big block.
  buf.clear();
  containerbuf<std::string> sib2{data};
  buf.resize(data.size());
  auto got = sib2.sgetn(&buf[0], static_cast<std::streamsize>(buf.size()));
  CAF_CHECK_EQUAL(static_cast<size_t>(got), data.size());
  CAF_CHECK_EQUAL(buf.size(), data.size());
  CAF_CHECK(std::equal(buf.begin(), buf.end(), data.begin() /*, data.end() */));
}
