cmake.exe ^
  -S . ^
  -B build ^
  -G "NMake Makefiles" ^
  -C ".ci\debug-flags.cmake" ^
  -DCAF_ENABLE_ROBOT_TESTS=ON ^
  -DBUILD_SHARED_LIBS=OFF ^
  -DCMAKE_C_COMPILER=cl.exe ^
  -DCMAKE_CXX_COMPILER=cl.exe ^
  -DOPENSSL_ROOT_DIR="C:\Program Files\OpenSSL-Win64"

cmake.exe --build build --target install --config debug || exit \b 1
