#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <vector>

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

CAF_BEGIN_TYPE_ID_BLOCK(fan_out_request, first_custom_type_id)

  CAF_ADD_ATOM(fan_out_request, row_atom)
  CAF_ADD_ATOM(fan_out_request, column_atom)
  CAF_ADD_ATOM(fan_out_request, average_atom)

CAF_END_TYPE_ID_BLOCK(fan_out_request)

using std::endl;
using std::chrono::seconds;
using namespace caf;

/// A simple actor for storing an integer value.
using cell = typed_actor<
  // Writes a new value.
  result<void>(put_atom, int),
  // Reads the value.
  result<int>(get_atom)>;

/// An for storing a 2-dimensional matrix of integers.
using matrix = typed_actor<
  // Writes a new value to given cell (x-coordinate, y-coordinate, new-value).
  result<void>(put_atom, int, int, int),
  // Reads from given cell.
  result<int>(get_atom, int, int),
  // Computes the average for given row.
  result<double>(get_atom, average_atom, row_atom, int),
  // Computes the average for given column.
  result<double>(get_atom, average_atom, column_atom, int)>;

struct cell_state {
  int value = 0;
  static constexpr const char* name = "cell";
};

cell::behavior_type cell_actor(cell::stateful_pointer<cell_state> self) {
  return {
    [=](put_atom, int val) { self->state.value = val; },
    [=](get_atom) { return self->state.value; },
  };
}

struct matrix_state {
  using row_type = std::vector<cell>;
  std::vector<row_type> rows;
  static constexpr const char* name = "matrix";
};

matrix::behavior_type matrix_actor(matrix::stateful_pointer<matrix_state> self,
                                   int rows, int columns) {
  // Spawn all cells and return our behavior.
  self->state.rows.resize(rows);
  for (int row = 0; row < rows; ++row) {
    auto& row_vec = self->state.rows[row];
    row_vec.resize(columns);
    for (int column = 0; column < columns; ++column)
      row_vec[column] = self->spawn(cell_actor);
  }
  return {
    [=](put_atom put, int row, int column, int val) {
      assert(row < rows && column < columns);
      return self->delegate(self->state.rows[row][column], put, val);
    },
    [=](get_atom get, int row, int column) {
      assert(row < rows && column < columns);
      return self->delegate(self->state.rows[row][column], get);
    },
    [=](get_atom get, average_atom, row_atom, int row) {
      assert(row < rows);
      auto rp = self->make_response_promise<double>();
      auto& row_vec = self->state.rows[row];
      self->fan_out_request<policy::select_all>(row_vec, infinite, get)
        .then(
          [=](std::vector<int> xs) mutable {
            assert(xs.size() == static_cast<size_t>(columns));
            rp.deliver(std::accumulate(xs.begin(), xs.end(), 0.0) / columns);
          },
          [=](error& err) mutable { rp.deliver(std::move(err)); });
      return rp;
    },
    // --(rst-fan-out-begin)--
    [=](get_atom get, average_atom, column_atom, int column) {
      assert(column < columns);
      std::vector<cell> columns;
      columns.reserve(rows);
      auto& rows_vec = self->state.rows;
      for (int row = 0; row < rows; ++row)
        columns.emplace_back(rows_vec[row][column]);
      auto rp = self->make_response_promise<double>();
      self->fan_out_request<policy::select_all>(columns, infinite, get)
        .then(
          [=](std::vector<int> xs) mutable {
            assert(xs.size() == static_cast<size_t>(rows));
            rp.deliver(std::accumulate(xs.begin(), xs.end(), 0.0) / rows);
          },
          [=](error& err) mutable { rp.deliver(std::move(err)); });
      return rp;
    },
    // --(rst-fan-out-end)--
  };
}

std::ostream& operator<<(std::ostream& out, const expected<int>& x) {
  if (x)
    return out << *x;
  return out << to_string(x.error());
}

void caf_main(actor_system& sys) {
  // Spawn our matrix.
  static constexpr int rows = 3;
  static constexpr int columns = 6;
  auto mx = sys.spawn(matrix_actor, rows, columns);
  auto f = make_function_view(mx);
  // Set cells in our matrix to these values:
  //      2     4     8    16    32    64
  //      3     9    27    81   243   729
  //      4    16    64   256  1024  4096
  for (int row = 0; row < rows; ++row)
    for (int column = 0; column < columns; ++column)
      f(put_atom_v, row, column, (int) pow(row + 2, column + 1));
  // Print out matrix.
  for (int row = 0; row < rows; ++row) {
    for (int column = 0; column < columns; ++column)
      std::cout << std::setw(4) << f(get_atom_v, row, column) << ' ';
    std::cout << '\n';
  }
  // Print out AVG for each row and column.
  for (int row = 0; row < rows; ++row)
    std::cout << "AVG(row " << row
              << ") = " << f(get_atom_v, average_atom_v, row_atom_v, row)
              << '\n';
  for (int column = 0; column < columns; ++column)
    std::cout << "AVG(column " << column
              << ") = " << f(get_atom_v, average_atom_v, column_atom_v, column)
              << '\n';
}

CAF_MAIN(id_block::fan_out_request)
