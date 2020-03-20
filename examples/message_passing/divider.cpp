/******************************************************************************\
 * A very basic, interactive divider.                                         *
\******************************************************************************/

// Manual refs: 17-19, 49-59, 70-76 (MessagePassing);
//              17-47 (Error)

#include <iostream>

#include "caf/all.hpp"

using std::cout;
using std::endl;
using std::flush;
using namespace caf;

enum class math_error : uint8_t {
  division_by_zero = 1,
};

std::string to_string(math_error x) {
  switch (x) {
    case math_error::division_by_zero:
      return "division_by_zero";
    default:
      return "-unknown-error-";
  }
}

namespace caf {

template <>
struct error_category<math_error> {
  static constexpr uint8_t value = 101;
};

} // namespace caf

class config : public actor_system_config {
public:
  config() {
    auto renderer = [](uint8_t x, const message&) {
      return to_string(static_cast<math_error>(x));
    };
    add_error_category(caf::error_category<math_error>::value, renderer);
  }
};

using divider = typed_actor<result<double>(div_atom, double, double)>;

divider::behavior_type divider_impl() {
  return {
    [](div_atom, double x, double y) -> result<double> {
      if (y == 0.0)
        return math_error::division_by_zero;
      return x / y;
    },
  };
}

void caf_main(actor_system& system, const config&) {
  double x;
  double y;
  cout << "x: " << flush;
  std::cin >> x;
  cout << "y: " << flush;
  std::cin >> y;
  auto div = system.spawn(divider_impl);
  scoped_actor self{system};
  self->request(div, std::chrono::seconds(10), div_atom_v, x, y)
    .receive(
      [&](double z) { aout(self) << x << " / " << y << " = " << z << endl; },
      [&](const error& err) {
        aout(self) << "*** cannot compute " << x << " / " << y << " => "
                   << system.render(err) << endl;
      });
}

CAF_MAIN()
