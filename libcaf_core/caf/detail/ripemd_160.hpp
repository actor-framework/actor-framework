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
 * Based on http://homes.esat.kuleuven.be/~cosicart/ps/AB-9601/rmd160.h;
 * original header:
 *
 *    AUTHOR:   Antoon Bosselaers, ESAT-COSIC
 *    DATE:     1 March 1996
 *    VERSION:  1.0
 *
 *    Copyright (c) Katholieke Universiteit Leuven
 *    1996, All Rights Reserved
 *
 *  Conditions for use of the RIPEMD-160 Software
 *
 *  The RIPEMD-160 software is freely available for use under the terms and
 *  conditions described hereunder, which shall be deemed to be accepted by
 *  any user of the software and applicable on any use of the software:
 *
 *  1. K.U.Leuven Department of Electrical Engineering-ESAT/COSIC shall for
 *     all purposes be considered the owner of the RIPEMD-160 software and of
 *     all copyright, trade secret, patent or other intellectual property
 *     rights therein.
 *  2. The RIPEMD-160 software is provided on an "as is" basis without
 *     warranty of any sort, express or implied. K.U.Leuven makes no
 *     representation that the use of the software will not infringe any
 *     patent or proprietary right of third parties. User will indemnify
 *     K.U.Leuven and hold K.U.Leuven harmless from any claims or liabilities
 *     which may arise as a result of its use of the software. In no
 *     circumstances K.U.Leuven R&D will be held liable for any deficiency,
 *     fault or other mishappening with regard to the use or performance of
 *     the software.
 *  3. User agrees to give due credit to K.U.Leuven in scientific publications
 *     or communications in relation with the use of the RIPEMD-160 software
 *     as follows: RIPEMD-160 software written by Antoon Bosselaers,
 *     available at http://www.esat.kuleuven.be/~cosicart/ps/AB-9601/.
\******************************************************************************/

#ifndef CAF_DETAIL_RIPEMD_160_HPP
#define CAF_DETAIL_RIPEMD_160_HPP

#include <array>
#include <string>

namespace caf {
namespace detail {

/// Creates a hash from `data` using the RIPEMD-160 algorithm.
void ripemd_160(std::array<uint8_t, 20>& storage, const std::string& data);

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_RIPEMD_160_HPP
