call "c:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86_amd64

cmake.exe ^
  -S . ^
  -B build ^
  -G "Visual Studio 17 2022" ^
  -C ".ci\debug-flags.cmake" ^
  -DEXTRA_FLAGS=/MP%CIRRUS_CPU% ^
  -DBUILD_SHARED_LIBS=OFF ^
  -DCMAKE_C_COMPILER=cl.exe ^
  -DCMAKE_CXX_COMPILER=cl.exe ^
  -DOPENSSL_ROOT_DIR="C:\Program Files\OpenSSL-Win64"

cmake.exe --build build --target install --config debug || exit \b 1
