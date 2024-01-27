set PATH=%cd%\bin\Debug;%PATH%
ctest.exe --output-on-failure -C debug --test-dir build || exit \b 1
