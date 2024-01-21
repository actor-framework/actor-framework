set PATH=%cd%\lib\Debug;%PATH%
ctest --output-on-failure -C debug --test-dir build || exit \b 1
