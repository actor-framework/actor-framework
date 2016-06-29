/******************************************************************************\
 * A very basic, interactive divider.                                         *
\******************************************************************************/

// Manual refs: 19-25, 35-48, 68-77 (MessagePassing);
//              19-34, 50-58 (Error)

#include <iostream>

#include "caf/all.hpp"

using std::cout;
using std::endl;
using std::flush;
using namespace caf;

namespace {

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

using div_atom = atom_constant<atom("div")>;

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

class config : public actor_system_config {
public:
  config() {
    auto renderer = [](uint8_t x, atom_value, const message&) {
      return "math_error" + deep_to_string_as_tuple(static_cast<math_error>(x));
    };
    add_error_category(atom("math"), renderer);
  }
};

void caf_main(actor_system& system, const config&) {
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

} // namespace <anonymous>

CAF_MAIN()
