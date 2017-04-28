#include <iostream>

#include <cmath>
#include <vector>
#include <tuple>

#include <unistd.h>

#include "caf/all.hpp"

using namespace caf;
using std::endl;
using std::cout;
using std::cerr;
using std::string;

behavior fibonacci_fun(stateful_actor<int>* self) {
  self->state = -1;
  return {
    [=] (int n) -> result<int> {
      if (n <= 1)
        return n;
      auto fib_actor_a = self->spawn(fibonacci_fun);
      auto fib_actor_b = self->spawn(fibonacci_fun);
      auto rp = self->make_response_promise<int>();        
      auto f = [=] (int result) mutable {
        if (self->state < 0)
          self->state = result;
        else 
          rp.deliver(self->state + result);  
      };
      self->request(fib_actor_a, infinite, n-1).then(f);
      self->request(fib_actor_b, infinite, n-2).then(f);
      return rp;
    },
  };
}

class my_config : public actor_system_config {
public:
  int fib_num = 35;

  my_config() {
    opt_group{custom_options_, "global"}
    .add(fib_num, "fibnum,n", "set the fib num to be calculated");
  }
};

void caf_main(actor_system& system, const my_config& cfg){
  auto fib_actor = system.spawn(fibonacci_fun);
  scoped_actor self{system};
  self->request(fib_actor, infinite, cfg.fib_num).receive(
    [&](int result){
      cout << "result: " << result << endl; 
    },
    [&](error& err){
      cerr << "Error: " << system.render(err) << std::endl;
    }
  );
}

CAF_MAIN()

