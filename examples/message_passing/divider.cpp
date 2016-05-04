/******************************************************************************\
 * A very basic, interactive divider.                                         *
\******************************************************************************/

// This example is partially included in the manual, do not modify
// without updating the references in the *.tex files!
// Manual references: lines 18-24, 34-47, and 62-72 (MessagePassing.tex)

#include <iostream>

#include "caf/all.hpp"

using std::cout;
using std::endl;
using std::flush;
using namespace caf;

enum class math_error : uint8_t {
  division_by_zero = 1
};

error make_error(math_error x) {
  return {static_cast<uint8_t>(x), atom("math")};
}

std::string to_string(math_error x) {
  switch (x) {
    case math_error::division_by_zero:
      return "division_by_zero";
    default:
      return "-unknown-error-";
  }
}

using div_atom = atom_constant<atom("add")>;

using divider = typed_actor<replies_to<div_atom, double, double>::with<double>>;

divider::behavior_type divider_impl() {
  return {
    [](div_atom, double x, double y) -> result<double> {
      if (y == 0.0)
        return math_error::division_by_zero;
      return x / y;
    }
  };
}

int main() {
  auto renderer = [](uint8_t x) {
    return to_string(static_cast<math_error>(x));
  };
  actor_system_config cfg;
  cfg.add_error_category(atom("math"), renderer);
  actor_system system{cfg};
  double x;
  double y;
  cout << "x: " << flush;
  std::cin >> x;
  cout << "y: " << flush;
  std::cin >> y;
  auto div = system.spawn(divider_impl);
  scoped_actor self{system};
  self->request(div, std::chrono::seconds(10), div_atom::value, x, y).receive(
    [&](double z) {
      aout(self) << x << " / " << y << " = " << z << endl;
    },
    [&](const error& err) {
      aout(self) << "*** cannot compute " << x << " / " << y << " => "
                 << system.render(err) << endl;
    }
  );
}
