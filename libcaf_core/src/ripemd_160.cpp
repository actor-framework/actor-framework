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

/******************************************************************************\
 * Based on http://homes.esat.kuleuven.be/~cosicart/ps/AB-9601/rmd160.c;      *
 * original header:                                                           *
 *                                                                            *
 *    AUTHOR:   Antoon Bosselaers, ESAT-COSIC                                 *
 *    DATE:   1 March 1996                                                    *
 *    VERSION:  1.0                                                           *
 *                                                                            *
 *    Copyright (c) Katholieke Universiteit Leuven                            *
 *    1996, All Rights Reserved                                               *
 *                                                                            *
 *  Conditions for use of the RIPEMD-160 Software                             *
 *                                                                            *
 *  The RIPEMD-160 software is freely available for use under the terms and   *
 *  conditions described hereunder, which shall be deemed to be accepted by   *
 *  any user of the software and applicable on any use of the software:       *
 *                                                                            *
 *  1. K.U.Leuven Department of Electrical Engineering-ESAT/COSIC shall for   *
 *   all purposes be considered the owner of the RIPEMD-160 software and of   *
 *   all copyright, trade secret, patent or other intellectual property       *
 *   rights therein.                                                          *
 *  2. The RIPEMD-160 software is provided on an "as is" basis without        *
 *   warranty of any sort, express or implied. K.U.Leuven makes no            *
 *   representation that the use of the software will not infringe any        *
 *   patent or proprietary right of third parties. User will indemnify        *
 *   K.U.Leuven and hold K.U.Leuven harmless from any claims or liabilities   *
 *   which may arise as a result of its use of the software. In no            *
 *   circumstances K.U.Leuven R&D will be held liable for any deficiency,     *
 *   fault or other mishappening with regard to the use or performance of     *
 *   the software.                                                            *
 *  3. User agrees to give due credit to K.U.Leuven in scientific             *
 *   publications or communications in relation with the use of the           *
 *   RIPEMD-160 software as follows: RIPEMD-160 software written by           *
 *   Antoon Bosselaers,                                                       *
 *   available at http://www.esat.kuleuven.be/~cosicart/ps/AB-9601/.          *
 *                                                                            *
\******************************************************************************/

#include <cstring>
#include "caf/detail/ripemd_160.hpp"

namespace {

// typedef 8 and 32 bit types, resp.
// adapt these, if necessary, for your operating system and compiler
using byte = unsigned char;
using dword = uint32_t;

static_assert(sizeof(dword) == sizeof(unsigned), "platform not supported");

// macro definitions

// collect four bytes into one word:
#define BYTES_TO_DWORD(strptr)                                                 \
  ((static_cast<dword>(*((strptr) + 3)) << 24)                                 \
   | (static_cast<dword>(*((strptr) + 2)) << 16)                               \
   | (static_cast<dword>(*((strptr) + 1)) << 8)                                \
   | (static_cast<dword>(*(strptr))))

// ROL(x, n) cyclically rotates x over n bits to the left
// x must be of an unsigned 32 bits type and 0 <= n < 32.
#define ROL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

// the five basic functions F(), G() and H()
#define F(x, y, z) ((x) ^ (y) ^ (z))
#define G(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define H(x, y, z) (((x) | ~(y)) ^ (z))
#define I(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define J(x, y, z) ((x) ^ ((y) | ~(z)))

// the ten basic operations FF() through III()
#define FF(a, b, c, d, e, x, s)                                                \
  {                                                                            \
    (a) += F((b), (c), (d)) + (x);                                             \
    (a) = ROL((a), (s)) + (e);                                                 \
    (c) = ROL((c), 10);                                                        \
  }

#define GG(a, b, c, d, e, x, s)                                                \
  {                                                                            \
    (a) += G((b), (c), (d)) + (x) + 0x5a827999U;                               \
    (a) = ROL((a), (s)) + (e);                                                 \
    (c) = ROL((c), 10);                                                        \
  }

#define HH(a, b, c, d, e, x, s)                                                \
  {                                                                            \
    (a) += H((b), (c), (d)) + (x) + 0x6ed9eba1U;                               \
    (a) = ROL((a), (s)) + (e);                                                 \
    (c) = ROL((c), 10);                                                        \
  }

#define II(a, b, c, d, e, x, s)                                                \
  {                                                                            \
    (a) += I((b), (c), (d)) + (x) + 0x8f1bbcdcU;                               \
    (a) = ROL((a), (s)) + (e);                                                 \
    (c) = ROL((c), 10);                                                        \
  }

#define JJ(a, b, c, d, e, x, s)                                                \
  {                                                                            \
    (a) += J((b), (c), (d)) + (x) + 0xa953fd4eU;                               \
    (a) = ROL((a), (s)) + (e);                                                 \
    (c) = ROL((c), 10);                                                        \
  }

#define FFF(a, b, c, d, e, x, s)                                               \
  {                                                                            \
    (a) += F((b), (c), (d)) + (x);                                             \
    (a) = ROL((a), (s)) + (e);                                                 \
    (c) = ROL((c), 10);                                                        \
  }

#define GGG(a, b, c, d, e, x, s)                                               \
  {                                                                            \
    (a) += G((b), (c), (d)) + (x) + 0x7a6d76e9U;                               \
    (a) = ROL((a), (s)) + (e);                                                 \
    (c) = ROL((c), 10);                                                        \
  }

#define HHH(a, b, c, d, e, x, s)                                               \
  {                                                                            \
    (a) += H((b), (c), (d)) + (x) + 0x6d703ef3U;                               \
    (a) = ROL((a), (s)) + (e);                                                 \
    (c) = ROL((c), 10);                                                        \
  }

#define III(a, b, c, d, e, x, s)                                               \
  {                                                                            \
    (a) += I((b), (c), (d)) + (x) + 0x5c4dd124U;                               \
    (a) = ROL((a), (s)) + (e);                                                 \
    (c) = ROL((c), 10);                                                        \
  }

#define JJJ(a, b, c, d, e, x, s)                                               \
  {                                                                            \
    (a) += J((b), (c), (d)) + (x) + 0x50a28be6U;                               \
    (a) = ROL((a), (s)) + (e);                                                 \
    (c) = ROL((c), 10);                                                        \
  }

void MDinit(dword* MDbuf) {
  MDbuf[0] = 0x67452301UL;
  MDbuf[1] = 0xefcdab89UL;
  MDbuf[2] = 0x98badcfeUL;
  MDbuf[3] = 0x10325476UL;
  MDbuf[4] = 0xc3d2e1f0UL;
}

void compress(dword* MDbuf, dword* X) {
  // round 1-5 variables
  dword aa = MDbuf[0];
  dword bb = MDbuf[1];
  dword cc = MDbuf[2];
  dword dd = MDbuf[3];
  dword ee = MDbuf[4];
  // parallel round 1-5 variables
  dword aaa = MDbuf[0];
  dword bbb = MDbuf[1];
  dword ccc = MDbuf[2];
  dword ddd = MDbuf[3];
  dword eee = MDbuf[4];
  // round 1
  FF(aa, bb, cc, dd, ee, X[0], 11);
  FF(ee, aa, bb, cc, dd, X[1], 14);
  FF(dd, ee, aa, bb, cc, X[2], 15);
  FF(cc, dd, ee, aa, bb, X[3], 12);
  FF(bb, cc, dd, ee, aa, X[4], 5);
  FF(aa, bb, cc, dd, ee, X[5], 8);
  FF(ee, aa, bb, cc, dd, X[6], 7);
  FF(dd, ee, aa, bb, cc, X[7], 9);
  FF(cc, dd, ee, aa, bb, X[8], 11);
  FF(bb, cc, dd, ee, aa, X[9], 13);
  FF(aa, bb, cc, dd, ee, X[10], 14);
  FF(ee, aa, bb, cc, dd, X[11], 15);
  FF(dd, ee, aa, bb, cc, X[12], 6);
  FF(cc, dd, ee, aa, bb, X[13], 7);
  FF(bb, cc, dd, ee, aa, X[14], 9);
  FF(aa, bb, cc, dd, ee, X[15], 8);
  // round 2
  GG(ee, aa, bb, cc, dd, X[7], 7);
  GG(dd, ee, aa, bb, cc, X[4], 6);
  GG(cc, dd, ee, aa, bb, X[13], 8);
  GG(bb, cc, dd, ee, aa, X[1], 13);
  GG(aa, bb, cc, dd, ee, X[10], 11);
  GG(ee, aa, bb, cc, dd, X[6], 9);
  GG(dd, ee, aa, bb, cc, X[15], 7);
  GG(cc, dd, ee, aa, bb, X[3], 15);
  GG(bb, cc, dd, ee, aa, X[12], 7);
  GG(aa, bb, cc, dd, ee, X[0], 12);
  GG(ee, aa, bb, cc, dd, X[9], 15);
  GG(dd, ee, aa, bb, cc, X[5], 9);
  GG(cc, dd, ee, aa, bb, X[2], 11);
  GG(bb, cc, dd, ee, aa, X[14], 7);
  GG(aa, bb, cc, dd, ee, X[11], 13);
  GG(ee, aa, bb, cc, dd, X[8], 12);
  // round 3
  HH(dd, ee, aa, bb, cc, X[3], 11);
  HH(cc, dd, ee, aa, bb, X[10], 13);
  HH(bb, cc, dd, ee, aa, X[14], 6);
  HH(aa, bb, cc, dd, ee, X[4], 7);
  HH(ee, aa, bb, cc, dd, X[9], 14);
  HH(dd, ee, aa, bb, cc, X[15], 9);
  HH(cc, dd, ee, aa, bb, X[8], 13);
  HH(bb, cc, dd, ee, aa, X[1], 15);
  HH(aa, bb, cc, dd, ee, X[2], 14);
  HH(ee, aa, bb, cc, dd, X[7], 8);
  HH(dd, ee, aa, bb, cc, X[0], 13);
  HH(cc, dd, ee, aa, bb, X[6], 6);
  HH(bb, cc, dd, ee, aa, X[13], 5);
  HH(aa, bb, cc, dd, ee, X[11], 12);
  HH(ee, aa, bb, cc, dd, X[5], 7);
  HH(dd, ee, aa, bb, cc, X[12], 5);
  // round 4
  II(cc, dd, ee, aa, bb, X[1], 11);
  II(bb, cc, dd, ee, aa, X[9], 12);
  II(aa, bb, cc, dd, ee, X[11], 14);
  II(ee, aa, bb, cc, dd, X[10], 15);
  II(dd, ee, aa, bb, cc, X[0], 14);
  II(cc, dd, ee, aa, bb, X[8], 15);
  II(bb, cc, dd, ee, aa, X[12], 9);
  II(aa, bb, cc, dd, ee, X[4], 8);
  II(ee, aa, bb, cc, dd, X[13], 9);
  II(dd, ee, aa, bb, cc, X[3], 14);
  II(cc, dd, ee, aa, bb, X[7], 5);
  II(bb, cc, dd, ee, aa, X[15], 6);
  II(aa, bb, cc, dd, ee, X[14], 8);
  II(ee, aa, bb, cc, dd, X[5], 6);
  II(dd, ee, aa, bb, cc, X[6], 5);
  II(cc, dd, ee, aa, bb, X[2], 12);
  // round 5
  JJ(bb, cc, dd, ee, aa, X[4], 9);
  JJ(aa, bb, cc, dd, ee, X[0], 15);
  JJ(ee, aa, bb, cc, dd, X[5], 5);
  JJ(dd, ee, aa, bb, cc, X[9], 11);
  JJ(cc, dd, ee, aa, bb, X[7], 6);
  JJ(bb, cc, dd, ee, aa, X[12], 8);
  JJ(aa, bb, cc, dd, ee, X[2], 13);
  JJ(ee, aa, bb, cc, dd, X[10], 12);
  JJ(dd, ee, aa, bb, cc, X[14], 5);
  JJ(cc, dd, ee, aa, bb, X[1], 12);
  JJ(bb, cc, dd, ee, aa, X[3], 13);
  JJ(aa, bb, cc, dd, ee, X[8], 14);
  JJ(ee, aa, bb, cc, dd, X[11], 11);
  JJ(dd, ee, aa, bb, cc, X[6], 8);
  JJ(cc, dd, ee, aa, bb, X[15], 5);
  JJ(bb, cc, dd, ee, aa, X[13], 6);
  // parallel round 1
  JJJ(aaa, bbb, ccc, ddd, eee, X[5], 8);
  JJJ(eee, aaa, bbb, ccc, ddd, X[14], 9);
  JJJ(ddd, eee, aaa, bbb, ccc, X[7], 9);
  JJJ(ccc, ddd, eee, aaa, bbb, X[0], 11);
  JJJ(bbb, ccc, ddd, eee, aaa, X[9], 13);
  JJJ(aaa, bbb, ccc, ddd, eee, X[2], 15);
  JJJ(eee, aaa, bbb, ccc, ddd, X[11], 15);
  JJJ(ddd, eee, aaa, bbb, ccc, X[4], 5);
  JJJ(ccc, ddd, eee, aaa, bbb, X[13], 7);
  JJJ(bbb, ccc, ddd, eee, aaa, X[6], 7);
  JJJ(aaa, bbb, ccc, ddd, eee, X[15], 8);
  JJJ(eee, aaa, bbb, ccc, ddd, X[8], 11);
  JJJ(ddd, eee, aaa, bbb, ccc, X[1], 14);
  JJJ(ccc, ddd, eee, aaa, bbb, X[10], 14);
  JJJ(bbb, ccc, ddd, eee, aaa, X[3], 12);
  JJJ(aaa, bbb, ccc, ddd, eee, X[12], 6);
  // parallel round 2
  III(eee, aaa, bbb, ccc, ddd, X[6], 9);
  III(ddd, eee, aaa, bbb, ccc, X[11], 13);
  III(ccc, ddd, eee, aaa, bbb, X[3], 15);
  III(bbb, ccc, ddd, eee, aaa, X[7], 7);
  III(aaa, bbb, ccc, ddd, eee, X[0], 12);
  III(eee, aaa, bbb, ccc, ddd, X[13], 8);
  III(ddd, eee, aaa, bbb, ccc, X[5], 9);
  III(ccc, ddd, eee, aaa, bbb, X[10], 11);
  III(bbb, ccc, ddd, eee, aaa, X[14], 7);
  III(aaa, bbb, ccc, ddd, eee, X[15], 7);
  III(eee, aaa, bbb, ccc, ddd, X[8], 12);
  III(ddd, eee, aaa, bbb, ccc, X[12], 7);
  III(ccc, ddd, eee, aaa, bbb, X[4], 6);
  III(bbb, ccc, ddd, eee, aaa, X[9], 15);
  III(aaa, bbb, ccc, ddd, eee, X[1], 13);
  III(eee, aaa, bbb, ccc, ddd, X[2], 11);
  // parallel round 3
  HHH(ddd, eee, aaa, bbb, ccc, X[15], 9);
  HHH(ccc, ddd, eee, aaa, bbb, X[5], 7);
  HHH(bbb, ccc, ddd, eee, aaa, X[1], 15);
  HHH(aaa, bbb, ccc, ddd, eee, X[3], 11);
  HHH(eee, aaa, bbb, ccc, ddd, X[7], 8);
  HHH(ddd, eee, aaa, bbb, ccc, X[14], 6);
  HHH(ccc, ddd, eee, aaa, bbb, X[6], 6);
  HHH(bbb, ccc, ddd, eee, aaa, X[9], 14);
  HHH(aaa, bbb, ccc, ddd, eee, X[11], 12);
  HHH(eee, aaa, bbb, ccc, ddd, X[8], 13);
  HHH(ddd, eee, aaa, bbb, ccc, X[12], 5);
  HHH(ccc, ddd, eee, aaa, bbb, X[2], 14);
  HHH(bbb, ccc, ddd, eee, aaa, X[10], 13);
  HHH(aaa, bbb, ccc, ddd, eee, X[0], 13);
  HHH(eee, aaa, bbb, ccc, ddd, X[4], 7);
  HHH(ddd, eee, aaa, bbb, ccc, X[13], 5);
  // parallel round 4
  GGG(ccc, ddd, eee, aaa, bbb, X[8], 15);
  GGG(bbb, ccc, ddd, eee, aaa, X[6], 5);
  GGG(aaa, bbb, ccc, ddd, eee, X[4], 8);
  GGG(eee, aaa, bbb, ccc, ddd, X[1], 11);
  GGG(ddd, eee, aaa, bbb, ccc, X[3], 14);
  GGG(ccc, ddd, eee, aaa, bbb, X[11], 14);
  GGG(bbb, ccc, ddd, eee, aaa, X[15], 6);
  GGG(aaa, bbb, ccc, ddd, eee, X[0], 14);
  GGG(eee, aaa, bbb, ccc, ddd, X[5], 6);
  GGG(ddd, eee, aaa, bbb, ccc, X[12], 9);
  GGG(ccc, ddd, eee, aaa, bbb, X[2], 12);
  GGG(bbb, ccc, ddd, eee, aaa, X[13], 9);
  GGG(aaa, bbb, ccc, ddd, eee, X[9], 12);
  GGG(eee, aaa, bbb, ccc, ddd, X[7], 5);
  GGG(ddd, eee, aaa, bbb, ccc, X[10], 15);
  GGG(ccc, ddd, eee, aaa, bbb, X[14], 8);
  // parallel round 5
  FFF(bbb, ccc, ddd, eee, aaa, X[12], 8);
  FFF(aaa, bbb, ccc, ddd, eee, X[15], 5);
  FFF(eee, aaa, bbb, ccc, ddd, X[10], 12);
  FFF(ddd, eee, aaa, bbb, ccc, X[4], 9);
  FFF(ccc, ddd, eee, aaa, bbb, X[1], 12);
  FFF(bbb, ccc, ddd, eee, aaa, X[5], 5);
  FFF(aaa, bbb, ccc, ddd, eee, X[8], 14);
  FFF(eee, aaa, bbb, ccc, ddd, X[7], 6);
  FFF(ddd, eee, aaa, bbb, ccc, X[6], 8);
  FFF(ccc, ddd, eee, aaa, bbb, X[2], 13);
  FFF(bbb, ccc, ddd, eee, aaa, X[13], 6);
  FFF(aaa, bbb, ccc, ddd, eee, X[14], 5);
  FFF(eee, aaa, bbb, ccc, ddd, X[0], 15);
  FFF(ddd, eee, aaa, bbb, ccc, X[3], 13);
  FFF(ccc, ddd, eee, aaa, bbb, X[9], 11);
  FFF(bbb, ccc, ddd, eee, aaa, X[11], 11);
  // combine results
  ddd += cc + MDbuf[1]; // final result for MDbuf[0]
  MDbuf[1] = MDbuf[2] + dd + eee;
  MDbuf[2] = MDbuf[3] + ee + aaa;
  MDbuf[3] = MDbuf[4] + aa + bbb;
  MDbuf[4] = MDbuf[0] + bb + ccc;
  MDbuf[0] = ddd;
}

void MDfinish(dword* MDbuf, const byte* strptr, dword lswlen, dword mswlen) {
  dword X[16]; // message words
  memset(X, 0, 16 * sizeof(dword));
  // put bytes from strptr into X
  for (unsigned int i = 0; i < (lswlen & 63); ++i) {
    // byte i goes into word X[i div 4] at pos.  8*(i mod 4)
    X[i >> 2] ^= static_cast<dword>(*strptr++) << (8 * (i & 3));
  }
  // append the bit n_ == 1
  X[(lswlen >> 2) & 15] ^= static_cast<dword>(1) << (8 * (lswlen & 3) + 7);
  if ((lswlen & 63) > 55) {
    // length goes to next block
    compress(MDbuf, X);
    memset(X, 0, 16 * sizeof(dword));
  }
  // append length in bits
  X[14] = lswlen << 3;
  X[15] = (lswlen >> 29) | (mswlen << 3);
  compress(MDbuf, X);
}

} // namespace <anonmyous>

namespace caf {
namespace detail {

void ripemd_160(std::array<uint8_t, 20>& storage, const std::string& data) {
  dword MDbuf[5]; // contains (A, B, C, D(, E))
  dword X[16];    // current 16-word chunk
  dword length;   // length in bytes of message
  auto message = reinterpret_cast<const unsigned char*>(data.c_str());
  // initialize
  MDinit(MDbuf);
  length = static_cast<dword>(data.size());
  // process message in 16-word chunks
  for (dword nbytes = length; nbytes > 63; nbytes -= 64) {
    for (dword i = 0; i < 16; ++i) {
      X[i] = BYTES_TO_DWORD(message);
      message += 4;
    }
    compress(MDbuf, X);
  }
  // length mod 64 bytes left
  // finish:
  MDfinish(MDbuf, message, length, 0);
  for (size_t i = 0; i < storage.size(); i += 4) {
    // extracts the 8 least significant bits by casting to byte
    storage[i] = static_cast<uint8_t>(MDbuf[i >> 2]);
    storage[i + 1] = static_cast<uint8_t>(MDbuf[i >> 2] >> 8);
    storage[i + 2] = static_cast<uint8_t>(MDbuf[i >> 2] >> 16);
    storage[i + 3] = static_cast<uint8_t>(MDbuf[i >> 2] >> 24);
  }
}

} // namespace detail
} // namespace caf
