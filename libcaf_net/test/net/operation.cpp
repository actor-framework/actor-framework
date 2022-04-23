#define CAF_SUITE net.operation

#include "caf/net/operation.hpp"

#include "net-test.hpp"

using namespace caf;
using namespace caf::net;

SCENARIO("add_read_flag adds the read bit unless block_read prevents it") {
  GIVEN("a valid operation enum value") {
    WHEN("calling add_read_flag") {
      THEN("the read bit is set unless the block_read flag is present") {
        auto add_rd = [](auto x) { return add_read_flag(x); };
        CHECK_EQ(add_rd(operation::none), operation::read);
        CHECK_EQ(add_rd(operation::read), operation::read);
        CHECK_EQ(add_rd(operation::write), operation::read_write);
        CHECK_EQ(add_rd(operation::block_read), operation::block_read);
        CHECK_EQ(add_rd(operation::block_write), operation::read_only);
        CHECK_EQ(add_rd(operation::read_write), operation::read_write);
        CHECK_EQ(add_rd(operation::read_only), operation::read_only);
        CHECK_EQ(add_rd(operation::write_only), operation::write_only);
        CHECK_EQ(add_rd(operation::shutdown), operation::shutdown);
      }
    }
  }
}

SCENARIO("add_write_flag adds the write bit unless block_write prevents it") {
  GIVEN("a valid operation enum value") {
    WHEN("calling add_write_flag") {
      THEN("the write bit is set unless the block_write flag is present") {
        auto add_wr = [](auto x) { return add_write_flag(x); };
        CHECK_EQ(add_wr(operation::none), operation::write);
        CHECK_EQ(add_wr(operation::read), operation::read_write);
        CHECK_EQ(add_wr(operation::write), operation::write);
        CHECK_EQ(add_wr(operation::block_read), operation::write_only);
        CHECK_EQ(add_wr(operation::block_write), operation::block_write);
        CHECK_EQ(add_wr(operation::read_write), operation::read_write);
        CHECK_EQ(add_wr(operation::read_only), operation::read_only);
        CHECK_EQ(add_wr(operation::write_only), operation::write_only);
        CHECK_EQ(add_wr(operation::shutdown), operation::shutdown);
      }
    }
  }
}

SCENARIO("remove_read_flag erases the read flag") {
  GIVEN("a valid operation enum value") {
    WHEN("calling remove_read_flag") {
      THEN("the read bit is removed if present") {
        auto del_rd = [](auto x) { return remove_read_flag(x); };
        CHECK_EQ(del_rd(operation::none), operation::none);
        CHECK_EQ(del_rd(operation::read), operation::none);
        CHECK_EQ(del_rd(operation::write), operation::write);
        CHECK_EQ(del_rd(operation::block_read), operation::block_read);
        CHECK_EQ(del_rd(operation::block_write), operation::block_write);
        CHECK_EQ(del_rd(operation::read_write), operation::write);
        CHECK_EQ(del_rd(operation::read_only), operation::block_write);
        CHECK_EQ(del_rd(operation::write_only), operation::write_only);
        CHECK_EQ(del_rd(operation::shutdown), operation::shutdown);
      }
    }
  }
}

SCENARIO("remove_write_flag erases the write flag") {
  GIVEN("a valid operation enum value") {
    WHEN("calling remove_write_flag") {
      THEN("the write bit is removed if present") {
        auto del_wr = [](auto x) { return remove_write_flag(x); };
        CHECK_EQ(del_wr(operation::none), operation::none);
        CHECK_EQ(del_wr(operation::read), operation::read);
        CHECK_EQ(del_wr(operation::write), operation::none);
        CHECK_EQ(del_wr(operation::block_read), operation::block_read);
        CHECK_EQ(del_wr(operation::block_write), operation::block_write);
        CHECK_EQ(del_wr(operation::read_write), operation::read);
        CHECK_EQ(del_wr(operation::read_only), operation::read_only);
        CHECK_EQ(del_wr(operation::write_only), operation::block_read);
        CHECK_EQ(del_wr(operation::shutdown), operation::shutdown);
      }
    }
  }
}

SCENARIO("block_reads removes the read flag and sets the block read flag") {
  GIVEN("a valid operation enum value") {
    WHEN("calling block_reads") {
      THEN("the read bit is removed if present and block_read is set") {
        auto block_rd = [](auto x) { return block_reads(x); };
        CHECK_EQ(block_rd(operation::none), operation::block_read);
        CHECK_EQ(block_rd(operation::read), operation::block_read);
        CHECK_EQ(block_rd(operation::write), operation::write_only);
        CHECK_EQ(block_rd(operation::block_read), operation::block_read);
        CHECK_EQ(block_rd(operation::block_write), operation::shutdown);
        CHECK_EQ(block_rd(operation::read_write), operation::write_only);
        CHECK_EQ(block_rd(operation::read_only), operation::shutdown);
        CHECK_EQ(block_rd(operation::write_only), operation::write_only);
        CHECK_EQ(block_rd(operation::shutdown), operation::shutdown);
      }
    }
  }
}

SCENARIO("block_writes removes the write flag and sets the block write flag") {
  GIVEN("a valid operation enum value") {
    WHEN("calling block_writes") {
      THEN("the write bit is removed if present and block_write is set") {
        auto block_wr = [](auto x) { return block_writes(x); };
        CHECK_EQ(block_wr(operation::none), operation::block_write);
        CHECK_EQ(block_wr(operation::read), operation::read_only);
        CHECK_EQ(block_wr(operation::write), operation::block_write);
        CHECK_EQ(block_wr(operation::block_read), operation::shutdown);
        CHECK_EQ(block_wr(operation::block_write), operation::block_write);
        CHECK_EQ(block_wr(operation::read_write), operation::read_only);
        CHECK_EQ(block_wr(operation::read_only), operation::read_only);
        CHECK_EQ(block_wr(operation::write_only), operation::shutdown);
        CHECK_EQ(block_wr(operation::shutdown), operation::shutdown);
      }
    }
  }
}

SCENARIO("is-predicates check whether certain flags are present") {
  GIVEN("a valid operation enum value") {
    WHEN("calling is_reading") {
      THEN("the predicate returns true if the read bit is present") {
        CHECK_EQ(is_reading(operation::none), false);
        CHECK_EQ(is_reading(operation::read), true);
        CHECK_EQ(is_reading(operation::write), false);
        CHECK_EQ(is_reading(operation::block_read), false);
        CHECK_EQ(is_reading(operation::block_write), false);
        CHECK_EQ(is_reading(operation::read_write), true);
        CHECK_EQ(is_reading(operation::read_only), true);
        CHECK_EQ(is_reading(operation::write_only), false);
        CHECK_EQ(is_reading(operation::shutdown), false);
      }
    }
    WHEN("calling is_writing") {
      THEN("the predicate returns true if the write bit is present") {
        CHECK_EQ(is_writing(operation::none), false);
        CHECK_EQ(is_writing(operation::read), false);
        CHECK_EQ(is_writing(operation::write), true);
        CHECK_EQ(is_writing(operation::block_read), false);
        CHECK_EQ(is_writing(operation::block_write), false);
        CHECK_EQ(is_writing(operation::read_write), true);
        CHECK_EQ(is_writing(operation::read_only), false);
        CHECK_EQ(is_writing(operation::write_only), true);
        CHECK_EQ(is_writing(operation::shutdown), false);
      }
    }
    WHEN("calling is_idle") {
      THEN("the predicate returns true when neither reading nor writing") {
        CHECK_EQ(is_idle(operation::none), true);
        CHECK_EQ(is_idle(operation::read), false);
        CHECK_EQ(is_idle(operation::write), false);
        CHECK_EQ(is_idle(operation::block_read), true);
        CHECK_EQ(is_idle(operation::block_write), true);
        CHECK_EQ(is_idle(operation::read_write), false);
        CHECK_EQ(is_idle(operation::read_only), false);
        CHECK_EQ(is_idle(operation::write_only), false);
        CHECK_EQ(is_idle(operation::shutdown), true);
      }
    }
    WHEN("calling is_read_blocked") {
      THEN("the predicate returns when blocking reads") {
        CHECK_EQ(is_read_blocked(operation::none), false);
        CHECK_EQ(is_read_blocked(operation::read), false);
        CHECK_EQ(is_read_blocked(operation::write), false);
        CHECK_EQ(is_read_blocked(operation::block_read), true);
        CHECK_EQ(is_read_blocked(operation::block_write), false);
        CHECK_EQ(is_read_blocked(operation::read_write), false);
        CHECK_EQ(is_read_blocked(operation::read_only), false);
        CHECK_EQ(is_read_blocked(operation::write_only), true);
        CHECK_EQ(is_read_blocked(operation::shutdown), true);
      }
    }
    WHEN("calling is_write_blocked") {
      THEN("the predicate returns when blocking writes") {
        CHECK_EQ(is_write_blocked(operation::none), false);
        CHECK_EQ(is_write_blocked(operation::read), false);
        CHECK_EQ(is_write_blocked(operation::write), false);
        CHECK_EQ(is_write_blocked(operation::block_read), false);
        CHECK_EQ(is_write_blocked(operation::block_write), true);
        CHECK_EQ(is_write_blocked(operation::read_write), false);
        CHECK_EQ(is_write_blocked(operation::read_only), true);
        CHECK_EQ(is_write_blocked(operation::write_only), false);
        CHECK_EQ(is_write_blocked(operation::shutdown), true);
      }
    }
  }
}
