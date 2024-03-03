// A very basic, interactive divider that showcases how to return an error with
// custom error code from a message handler.

#include "caf/actor_ostream.hpp"
#include "caf/actor_system.hpp"
#include "caf/caf_main.hpp"
#include "caf/default_enum_inspect.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/typed_event_based_actor.hpp"

#include <iostream>

using namespace std::literals;

// --(rst-math-error-begin)--
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

bool from_string(std::string_view in, math_error& out) {
  if (in == "division_by_zero") {
    out = math_error::division_by_zero;
    return true;
  } else {
    return false;
  }
}

bool from_integer(uint8_t in, math_error& out) {
  if (in == 1) {
    out = math_error::division_by_zero;
    return true;
  } else {
    return false;
  }
}

template <class Inspector>
bool inspect(Inspector& f, math_error& x) {
  return caf::default_enum_inspect(f, x);
}

CAF_BEGIN_TYPE_ID_BLOCK(divider, first_custom_type_id)

  CAF_ADD_TYPE_ID(divider, (math_error))

CAF_END_TYPE_ID_BLOCK(divider)

CAF_ERROR_CODE_ENUM(math_error)
// --(rst-math-error-end)--

using std::cout;
using std::endl;
using std::flush;

using namespace caf;

// --(rst-divider-begin)--
struct divider_trait {
  using signatures = type_list<result<double>(div_atom, double, double)>;
};

using divider = typed_actor<divider_trait>;

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
  self->mail(div_atom_v, x, y)
    .request(div, 10s)
    .receive([&](double z) { aout(self).println("{} / {} = {}", x, y, z); },
             [&](const error& err) {
               aout(self).println("*** cannot compute {} / {} => {}", x, y,
                                  err);
             });
  // --(rst-request-end)--
}

CAF_MAIN(id_block::divider)
