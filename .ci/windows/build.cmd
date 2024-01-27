cmake.exe ^
  -S . ^
  -B build ^
  -G "Visual Studio 17 2022" ^
  -C ".ci\debug-flags.cmake" ^
  -DCMAKE_C_COMPILER=cl.exe ^
  -DCMAKE_CXX_COMPILER=cl.exe ^
  -DCMAKE_CXX_FLAGS="/WX" ^
  -DPython3_ROOT_DIR=%Python3_ROOT_DIR% ^
  -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=%cd%\bin ^
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=%cd%\bin

cmake.exe --build build --parallel %NUMBER_OF_PROCESSORS% --target install --config debug || exit \b 1
