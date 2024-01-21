cmake.exe ^
  -S . ^
  -B build ^
  -G "Visual Studio 17 2022" ^
  -C ".ci\debug-flags.cmake" ^
  -DCAF_ENABLE_ROBOT_TESTS=ON ^
  -DCAF_LOG_LEVEL=TRACE ^
  -DCMAKE_C_COMPILER=cl.exe ^
  -DCMAKE_CXX_COMPILER=cl.exe ^
  -DCMAKE_CXX_FLAGS="/WX" ^
  -DPython3_ROOT_DIR=%Python3_ROOT_DIR% ^
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=lib

cmake.exe --build build --parallel %NUMBER_OF_PROCESSORS% --target install --config debug || exit \b 1
