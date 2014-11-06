#include <string>

#include "caf/optional.hpp"

#include "test.hpp"

using namespace std;
using namespace caf;

int main() {
  {
    optional<int> i,j;
    CAF_CHECK(i == j);
    CAF_CHECK(!(i != j));
  }
  {
    optional<int> i = 5;
    optional<int> j = 6;
    CAF_CHECK(!(i == j));
    CAF_CHECK(i != j);
  }
  {
    optional<int> i;
    optional<double> j;
    CAF_CHECK(i == j);
    CAF_CHECK(!(i != j));
  }
  {
    struct qwertz {
      qwertz(int i, int j) : m_i(i), m_j(j) {
        CAF_CHECKPOINT();
      }
      int m_i;
      int m_j;
    };
    {
      optional<qwertz> i;
      CAF_CHECK(i.empty());
    }
    {
      qwertz obj (1,2);
      optional<qwertz> j(obj);
      CAF_CHECK(!j.empty());
    }
    {
      optional<qwertz> i = qwertz(1,2);
      CAF_CHECK(!i.empty());
      optional<qwertz> j = { { 1, 2 } };
      CAF_CHECK(!j.empty());
    }
  }
  return CAF_TEST_RESULT();
}

