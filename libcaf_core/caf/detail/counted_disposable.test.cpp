// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/counted_disposable.hpp"

#include "caf/test/scenario.hpp"

#include "caf/disposable.hpp"
#include "caf/make_counted.hpp"

using namespace caf;

using caf::detail::counted_disposable;

SCENARIO("disposing all nested disposables disposes the counted_disposable") {
  GIVEN("a counted_disposable wrapping a flag disposable") {
    auto flag = disposable::make_flag();
    auto counted = make_counted<counted_disposable>(flag);
    WHEN("acquiring no nested disposables") {
      THEN("the flag is not disposed") {
        check(!flag.disposed());
        check(!counted->disposed());
      }
    }
    WHEN("acquiring a single nested disposable") {
      auto nested = counted->acquire();
      THEN("the flag is still not disposed") {
        check(!flag.disposed());
        check(!counted->disposed());
        check(!nested.disposed());
      }
    }
    WHEN("acquiring a single nested disposable and disposing it") {
      auto nested = counted->acquire();
      nested.dispose();
      THEN("the flag becomes disposed") {
        check(flag.disposed());
        check(counted->disposed());
        check(nested.disposed());
      }
    }
    WHEN("acquiring multiple nested disposables") {
      auto nested1 = counted->acquire();
      auto nested2 = counted->acquire();
      auto nested3 = counted->acquire();
      THEN("the flag is still not disposed") {
        check(!flag.disposed());
        check(!counted->disposed());
        check(!nested1.disposed());
        check(!nested2.disposed());
        check(!nested3.disposed());
      }
    }
    WHEN("acquiring multiple nested disposables and disposing one") {
      auto nested1 = counted->acquire();
      auto nested2 = counted->acquire();
      auto nested3 = counted->acquire();
      nested1.dispose();
      THEN("the flag is still not disposed") {
        check(!flag.disposed());
        check(!counted->disposed());
        check(nested1.disposed());
        check(!nested2.disposed());
        check(!nested3.disposed());
      }
    }
    WHEN("acquiring multiple nested disposables and disposing all but one") {
      auto nested1 = counted->acquire();
      auto nested2 = counted->acquire();
      auto nested3 = counted->acquire();
      nested1.dispose();
      nested2.dispose();
      THEN("the flag is still not disposed") {
        check(!flag.disposed());
        check(!counted->disposed());
        check(nested1.disposed());
        check(nested2.disposed());
        check(!nested3.disposed());
      }
    }
    WHEN("acquiring multiple nested disposables and disposing all") {
      auto nested1 = counted->acquire();
      auto nested2 = counted->acquire();
      auto nested3 = counted->acquire();
      nested1.dispose();
      nested2.dispose();
      nested3.dispose();
      THEN("the flag becomes disposed") {
        check(flag.disposed());
        check(counted->disposed());
        check(nested1.disposed());
        check(nested2.disposed());
        check(nested3.disposed());
      }
    }
    WHEN("disposing the counted_disposable directly") {
      counted->dispose();
      THEN("the flag becomes disposed immediately") {
        check(flag.disposed());
        check(counted->disposed());
      }
    }
  }
}

SCENARIO("counted_disposable allows safe double disposal") {
  GIVEN("a counted_disposable wrapping a flag disposable") {
    auto flag = disposable::make_flag();
    auto counted = make_counted<counted_disposable>(flag);
    WHEN("disposing a nested disposable multiple times") {
      auto nested = counted->acquire();
      THEN("multiple disposals are safe") {
        check(!flag.disposed());
        nested.dispose();
        check(flag.disposed());
        nested.dispose(); // Should be safe
        check(flag.disposed());
      }
    }
  }
}

SCENARIO("counted_disposable disposal order does not matter") {
  GIVEN("a counted_disposable wrapping a flag disposable") {
    auto flag = disposable::make_flag();
    auto counted = make_counted<counted_disposable>(flag);
    WHEN("disposing nested disposables in different orders") {
      auto nested1 = counted->acquire();
      auto nested2 = counted->acquire();
      auto nested3 = counted->acquire();
      THEN("disposal order does not matter") {
        // Dispose in reverse order
        nested3.dispose();
        check(!flag.disposed());
        nested1.dispose();
        check(!flag.disposed());
        nested2.dispose();
        check(flag.disposed());
      }
    }
  }
}

SCENARIO("nested disposables release their reference when going out of scope") {
  GIVEN("a counted_disposable wrapping a flag disposable") {
    auto flag = disposable::make_flag();
    auto counted = make_counted<counted_disposable>(flag);
    WHEN("letting nested disposables go out of scope") {
      THEN("they are automatically disposed") {
        {
          auto nested1 = counted->acquire();
          auto nested2 = counted->acquire();
          check(!flag.disposed());
        } // nested1 and nested2 go out of scope here
        check(flag.disposed());
      }
    }
  }
}

SCENARIO("multiple counted_disposable instances operate independently") {
  GIVEN("two independent counted_disposables") {
    auto flag1 = disposable::make_flag();
    auto flag2 = disposable::make_flag();
    auto counted1 = make_counted<counted_disposable>(flag1);
    auto counted2 = make_counted<counted_disposable>(flag2);
    WHEN("acquiring nested disposables from both instances") {
      auto nested1 = counted1->acquire();
      auto nested2 = counted2->acquire();
      THEN("they operate independently") {
        check(!flag1.disposed());
        check(!flag2.disposed());
        nested1.dispose();
        check(flag1.disposed());
        check(!flag2.disposed());
        nested2.dispose();
        check(flag1.disposed());
        check(flag2.disposed());
      }
    }
  }
}
