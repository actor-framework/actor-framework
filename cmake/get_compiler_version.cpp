#include <iostream>
using namespace std;

int main() {
#   ifdef __clang__
    cout << __clang_major__
         << "."
         << __clang_minor__;
#   elif defined(__GNUC__)
    cout << __GNUC__
         << "."
         << __GNUC_MINOR__;
#   else
    cout << "0.0";
#   endif
    return 0;
}
