#include "caf/actor_from_state.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/exec_main.hpp"
#include "caf/function_view.hpp"
#include "caf/init_global_meta_objects.hpp"
#include "caf/policy/select_all.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/typed_actor.hpp"
#include "caf/typed_event_based_actor.hpp"

#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <vector>

CAF_BEGIN_TYPE_ID_BLOCK(fan_out_request, first_custom_type_id)

  CAF_ADD_ATOM(fan_out_request, row_atom)
  CAF_ADD_ATOM(fan_out_request, column_atom)
  CAF_ADD_ATOM(fan_out_request, average_atom)

CAF_END_TYPE_ID_BLOCK(fan_out_request)

using std::endl;
using std::chrono::seconds;
using namespace caf;

/// A simple actor for storing an integer value.
struct cell_trait {
  using signatures = type_list<
    // Writes a new value.
    result<void>(put_atom, int32_t),
    // Reads the value.
    result<int32_t>(get_atom)>;
};
using cell = typed_actor<cell_trait>;

/// An for storing a 2-dimensional matrix of integers.
struct matrix_trait {
  using signatures = type_list<
    // Writes a new value to given cell (x-coordinate, y-coordinate, new-value).
    result<void>(put_atom, uint32_t, uint32_t, int32_t),
    // Reads from given cell.
    result<int>(get_atom, uint32_t, uint32_t),
    // Computes the average for given row.
    result<double>(get_atom, average_atom, row_atom, uint32_t),
    // Computes the average for given column.
    result<double>(get_atom, average_atom, column_atom, uint32_t)>;
};
using matrix = typed_actor<matrix_trait>;

struct cell_state {
  explicit cell_state(cell::pointer selfptr) : self(selfptr) {
    // nop
  }

  cell::behavior_type make_behavior() {
    return {
      [this](put_atom, int32_t val) { value = val; },
      [this](get_atom) { return value; },
    };
  }

  cell::pointer self;
  int32_t value = 0;
  static constexpr const char* name = "cell";
};

struct matrix_state {
  matrix_state(matrix::pointer selfptr, size_t num_rows, size_t num_columns)
    : self(selfptr), rows(num_rows), columns(num_columns) {
    // Spawn all cells.
    data.resize(rows);
    for (auto& row : data) {
      row.resize(columns);
      for (auto& field : row)
        field = self->spawn(actor_from_state<cell_state>);
    }
  }

  matrix::behavior_type make_behavior() {
    return {
      [this](put_atom put, uint32_t row, uint32_t column,
             int32_t val) -> result<void> {
        if (row >= rows) {
          return error{sec::invalid_argument, "row out of range"};
        }
        if (column >= columns) {
          return error{sec::invalid_argument, "column out of range"};
        }
        return self->delegate(data[row][column], put, val);
      },
      [this](get_atom get, uint32_t row, uint32_t column) -> result<int32_t> {
        if (row >= rows) {
          return error{sec::invalid_argument, "row out of range"};
        }
        if (column >= columns) {
          return error{sec::invalid_argument, "column out of range"};
        }
        return self->delegate(data[row][column], get);
      },
      [this](get_atom get, average_atom, row_atom,
             uint32_t row) -> result<double> {
        if (row >= rows) {
          return error{sec::invalid_argument, "row out of range"};
        }
        auto rp = self->make_response_promise<double>();
        self->fan_out_request<policy::select_all>(data[row], infinite, get)
          .then(
            [this, rp](std::vector<int> xs) mutable {
              assert(xs.size() == columns);
              rp.deliver(std::accumulate(xs.begin(), xs.end(), 0.0) / columns);
            },
            [rp](error& err) mutable { rp.deliver(std::move(err)); });
        return rp;
      },
      // --(rst-fan-out-begin)--
      [this](get_atom get, average_atom, column_atom,
             uint32_t column) -> result<double> {
        if (column >= columns) {
          return error{sec::invalid_argument, "column out of range"};
        }
        auto cells = std::vector<cell>{}; // The cells we need to query.
        cells.reserve(rows);
        for (auto& row : data)
          cells.emplace_back(row[column]);
        auto rp = self->make_response_promise<double>();
        self->fan_out_request<policy::select_all>(cells, infinite, get)
          .then(
            [this, rp](std::vector<int> xs) mutable {
              assert(xs.size() == rows);
              rp.deliver(std::accumulate(xs.begin(), xs.end(), 0.0) / rows);
            },
            [rp](error& err) mutable { rp.deliver(std::move(err)); });
        return rp;
      },
      // --(rst-fan-out-end)--
    };
  }

  matrix::pointer self;
  using row_type = std::vector<cell>;
  size_t rows;
  size_t columns;
  std::vector<row_type> data;
  static constexpr const char* name = "matrix";
};

std::ostream& operator<<(std::ostream& out, const expected<int>& x) {
  if (x)
    return out << *x;
  return out << to_string(x.error());
}

void caf_main(actor_system& sys) {
  using std::cout;
  // Spawn our matrix.
  static constexpr size_t rows = 3;
  static constexpr size_t columns = 6;
  auto mx = sys.spawn(actor_from_state<matrix_state>, rows, columns);
  auto f = make_function_view(mx);
  // Set cells in our matrix to these values:
  //      2     4     8    16    32    64
  //      3     9    27    81   243   729
  //      4    16    64   256  1024  4096
  for (uint32_t row = 0; row < rows; ++row)
    for (uint32_t column = 0; column < columns; ++column)
      f(put_atom_v, row, column,
        static_cast<int32_t>(pow(row + 2, column + 1)));
  // Print out matrix.
  for (uint32_t row = 0; row < rows; ++row) {
    for (uint32_t column = 0; column < columns; ++column)
      cout << std::setw(4) << f(get_atom_v, row, column) << ' ';
    cout << std::endl;
  }
  // Print out AVG for each row and column.
  for (uint32_t row = 0; row < rows; ++row)
    cout << "AVG(row " << row << ") = "
         << caf::to_string(f(get_atom_v, average_atom_v, row_atom_v, row))
         << std::endl;
  for (uint32_t column = 0; column < columns; ++column)
    cout << "AVG(column " << column << ") = "
         << caf::to_string(f(get_atom_v, average_atom_v, column_atom_v, column))
         << std::endl;
}

CAF_MAIN(id_block::fan_out_request)
