

/*
 * this file does absolutely nothing right now 
 * should however test the actor facade spawn function 
 */

//#include <caf/all.hpp>  // Includes most CAF essentials

//#include "caf/opencl/actor_facade.hpp"
//#include "caf/opencl/manager.hpp"

//using namespace caf;

//runs a test to ensure the actor facade can spawn in
//correctly 

/*
 * For reference the creating executable of the cmake portion of these files
 * will only link caf tests to main.test.cpp so thats how you end up with linker errors
 *
void actor_facade_spawn_test() {

	actor_system_config cfg;

	actor_system sys{cfg};

	caf::opencl::manager mgr{sys};

	int x = 1;
	auto gpuActor = mgr.spawn(x);


 // actor_system_config cfg;
  //actor_system system{cfg};

  // Spawn actor_facade<int, std::string> passing 42 and "hello"
  //auto my_actor = system.spawn<caf::opencl::actor_facade<false,int, std::string>>(42, "hello");

}
*/



int main(void) {

	return 0;

}	
