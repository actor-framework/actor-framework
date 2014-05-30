/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/

/******************************************************************************\
 *            Based on http://beej.us/guide/bgnet/examples/pack2.c            *
\******************************************************************************/


#ifndef CPPA_DETAIL_IEEE_754_HPP
#define CPPA_DETAIL_IEEE_754_HPP

#include <cmath>

namespace cppa {
namespace detail {

template<typename T>
struct ieee_754_trait;

template<>
struct ieee_754_trait<float> {
    static constexpr std::uint32_t bits = 32;       // number of bits
    static constexpr std::uint32_t expbits = 8;     // bits used for exponent
    static constexpr float zero = 0.0f;             // the value 0
    static constexpr float p5 = 0.5f;               // the value 0.5
    using packed_type = std::uint32_t;              // unsigned integer type
    using signed_packed_type = std::int32_t;        // signed integer type
    using float_type = float;                       // floating point type
};

template<>
struct ieee_754_trait<std::uint32_t> : ieee_754_trait<float> { };

template<>
struct ieee_754_trait<double> {
    static constexpr std::uint64_t bits = 64;
    static constexpr std::uint64_t expbits = 11;
    static constexpr double  zero = 0.0;
    static constexpr double p5 = 0.5;
    using packed_type = std::uint64_t;
    using signed_packed_type = std::int64_t;
    using float_type = double;
};

template<>
struct ieee_754_trait<std::uint64_t> : ieee_754_trait<double> { };

template<typename T>
typename ieee_754_trait<T>::packed_type pack754(T f) {
    typedef ieee_754_trait<T> trait;
    typedef typename trait::packed_type result_type;
    // filter special type
    if (fabs(f) <= trait::zero) return 0; // only true if f equals +0 or -0
    auto significandbits = trait::bits - trait::expbits - 1; // -1 for sign bit
    // check sign and begin normalization
    result_type sign;
    T fnorm;
    if (f < 0) {
        sign = 1;
        fnorm = -f;
    }
    else {
        sign = 0;
        fnorm = f;
    }
    // get the normalized form of f and track the exponent
    typename ieee_754_trait<T>::packed_type shift = 0;
    while (fnorm >= static_cast<T>(2)) {
        fnorm /= static_cast<T>(2);
        ++shift;
    }
    while (fnorm < static_cast<T>(1)) {
        fnorm *= static_cast<T>(2);
        --shift;
    }
    fnorm = fnorm - static_cast<T>(1);
    // calculate 2^significandbits
    auto pownum = static_cast<result_type>(1) << significandbits;
    // calculate the binary form (non-float) of the significand data
    auto significand = static_cast<result_type>(fnorm * (pownum + trait::p5));
    // get the biased exponent
    auto exp = shift + ((1 << (trait::expbits - 1)) - 1); // shift + bias
    // return the final answer
    return (sign << (trait::bits - 1)) | (exp << (trait::bits - trait::expbits - 1)) | significand;
}


template<typename T>
typename ieee_754_trait<T>::float_type unpack754(T i) {
    typedef ieee_754_trait<T> trait;
    typedef typename trait::signed_packed_type signed_type;
    typedef typename trait::float_type result_type;
    if (i == 0) return trait::zero;
    auto significandbits = trait::bits - trait::expbits - 1; // -1 for sign bit
    // pull the significand
    result_type result = (i & ((static_cast<T>(1) << significandbits) - 1)); // mask
    result /= (static_cast<T>(1) << significandbits); // convert back to float
    result += static_cast<result_type>(1); // add the one back on
    // deal with the exponent
    auto si = static_cast<signed_type>(i);
    auto bias = (1 << (trait::expbits - 1)) - 1;
    auto pownum = static_cast<signed_type>(1) << trait::expbits;
    auto shift = static_cast<signed_type>(((si >> significandbits) & (pownum - 1)) - bias);
    while (shift > 0) {
        result *= static_cast<result_type>(2);
        --shift;
    }
    while (shift < 0) {
        result /= static_cast<result_type>(2);
        ++shift;
    }
    // sign it
    result *= (i >> (trait::bits - 1)) & 1 ? -1 : 1;
    return result;
}

} // namespace detail
} // namespace cppa

#endif // CPPA_DETAIL_IEEE_754_HPP
