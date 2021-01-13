// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdint>
#include <variant>
#include <vector>

#include "caf/detail/monotonic_buffer_resource.hpp"
#include "caf/parser_state.hpp"
#include "caf/string_view.hpp"

// This JSON abstraction is designed to allocate its entire state in a monotonic
// buffer resource. This minimizes memory allocations and also enables us to
// "wink out" the entire JSON object by simply reclaiming the memory without
// having to call a single destructor. The API is not optimized for convenience
// or safety, since the only place we use this API is the json_reader.

namespace caf::detail::json {

struct null_t {};

class value {
public:
  using array_allocator = monotonic_buffer_resource::allocator<value>;

  using array = std::vector<value, array_allocator>;

  struct member {
    string_view key;
    value* val = nullptr;
  };

  using member_allocator = monotonic_buffer_resource::allocator<member>;

  using object = std::vector<member, member_allocator>;

  using data_type
    = std::variant<null_t, int64_t, double, bool, string_view, array, object>;

  data_type data;
};

using array = value::array;

using member = value::member;

using object = value::object;

value* make_value(monotonic_buffer_resource* storage);

array* make_array(monotonic_buffer_resource* storage);

object* make_object(monotonic_buffer_resource* storage);

value* parse(string_parser_state& ps, monotonic_buffer_resource* storage);

} // namespace caf::detail::json
