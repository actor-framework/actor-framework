// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/type_list.hpp"

using std::is_same_v;

namespace caf::detail {

namespace {

struct sx {};
struct s1 {};
struct s2 {};
struct s3 {};
struct s4 {};

template <class R, class... Ts>
using rm_t = tl_remove_t<type_list<Ts...>, R>;

} // namespace

// Remove from empty list.
static_assert(is_same_v<rm_t<s1>, type_list<>>);

// Remove from single element list.
static_assert(is_same_v<rm_t<sx, sx>, type_list<>>);
static_assert(is_same_v<rm_t<sx, s1>, type_list<s1>>);

// Remove from two element list.
static_assert(is_same_v<rm_t<sx, s1, s2>, type_list<s1, s2>>);
static_assert(is_same_v<rm_t<sx, s1, sx>, type_list<s1>>);
static_assert(is_same_v<rm_t<sx, sx, s2>, type_list<s2>>);
static_assert(is_same_v<rm_t<sx, sx, sx>, type_list<>>);

// Remove from three element list.
static_assert(is_same_v<rm_t<sx, s1, s2, s3>, type_list<s1, s2, s3>>);
static_assert(is_same_v<rm_t<sx, s1, s2, sx>, type_list<s1, s2>>);
static_assert(is_same_v<rm_t<sx, s1, sx, s3>, type_list<s1, s3>>);
static_assert(is_same_v<rm_t<sx, s1, sx, sx>, type_list<s1>>);
static_assert(is_same_v<rm_t<sx, sx, s2, s3>, type_list<s2, s3>>);
static_assert(is_same_v<rm_t<sx, sx, s2, sx>, type_list<s2>>);
static_assert(is_same_v<rm_t<sx, sx, sx, s3>, type_list<s3>>);
static_assert(is_same_v<rm_t<sx, sx, sx, sx>, type_list<>>);

// Remove from four element list.
static_assert(is_same_v<rm_t<sx, s1, s2, s3, s4>, type_list<s1, s2, s3, s4>>);
static_assert(is_same_v<rm_t<sx, s1, s2, s3, sx>, type_list<s1, s2, s3>>);
static_assert(is_same_v<rm_t<sx, s1, s2, sx, s4>, type_list<s1, s2, s4>>);
static_assert(is_same_v<rm_t<sx, s1, s2, sx, sx>, type_list<s1, s2>>);
static_assert(is_same_v<rm_t<sx, s1, sx, s3, s4>, type_list<s1, s3, s4>>);
static_assert(is_same_v<rm_t<sx, s1, sx, s3, sx>, type_list<s1, s3>>);
static_assert(is_same_v<rm_t<sx, s1, sx, sx, s4>, type_list<s1, s4>>);
static_assert(is_same_v<rm_t<sx, s1, sx, sx, sx>, type_list<s1>>);
static_assert(is_same_v<rm_t<sx, sx, s2, s3, s4>, type_list<s2, s3, s4>>);
static_assert(is_same_v<rm_t<sx, sx, s2, s3, sx>, type_list<s2, s3>>);
static_assert(is_same_v<rm_t<sx, sx, s2, sx, s4>, type_list<s2, s4>>);
static_assert(is_same_v<rm_t<sx, sx, s2, sx, sx>, type_list<s2>>);
static_assert(is_same_v<rm_t<sx, sx, sx, s3, s4>, type_list<s3, s4>>);
static_assert(is_same_v<rm_t<sx, sx, sx, s3, sx>, type_list<s3>>);
static_assert(is_same_v<rm_t<sx, sx, sx, sx, s4>, type_list<s4>>);
static_assert(is_same_v<rm_t<sx, sx, sx, sx, sx>, type_list<>>);

} // namespace caf::detail
