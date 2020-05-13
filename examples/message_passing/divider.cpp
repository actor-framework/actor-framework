// A basic, interactive divider.

#include <iostream>

#include "caf/all.hpp"

using std::cout;
using std::endl;
using std::flush;
using namespace caf;

enum class math_error : uint8_t;

CAF_BEGIN_TYPE_ID_BLOCK(divider, first_custom_type_id)

  CAF_ADD_TYPE_ID(divider, (math_error))

CAF_END_TYPE_ID_BLOCK(divider)

// --(rst-divider-begin)--
enum class math_error : uint8_t {
  division_by_zero = 1,
};

CAF_ERROR_CODE_ENUM(math_error)

std::string to_string(math_error x) {
  switch (x) {
    case math_error::division_by_zero:
      return "division_by_zero";
    default:
      return "-unknown-error-";
  }
}

using divider = typed_actor<replies_to<div_atom, double, double>::with<double>>;

divider::behavior_type divider_impl() {
  return {
    [](div_atom, double x, double y) -> result<double> {
      if (y == 0.0)
        return math_error::division_by_zero;
      return x / y;
    },
  };
}
// --(rst-divider-end)--

void caf_main(actor_system& system) {
  double x;
  double y;
  cout << "x: " << flush;
  std::cin >> x;
  cout << "y: " << flush;
  std::cin >> y;
  // --(rst-request-begin)--
  auto div = system.spawn(divider_impl);
  scoped_actor self{system};
  self->request(div, std::chrono::seconds(10), div_atom_v, x, y)
    .receive(
      [&](double z) { aout(self) << x << " / " << y << " = " << z << endl; },
      [&](const error& err) {
        aout(self) << "*** cannot compute " << x << " / " << y << " => "
                   << to_string(err) << endl;
      });
  // --(rst-request-end)--
}

CAF_MAIN(id_block::divider)
