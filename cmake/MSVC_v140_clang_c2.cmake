## FLAGS FOR MSVC v140_clang_c2
set(CMAKE_CXX_FLAGS_DEBUG_INIT          "/D_DEBUG /MTd /GR -Od -g2 -gdwarf-2 -std=c++1z -fms-extensions -fexceptions")
set(CMAKE_CXX_FLAGS_MINSIZEREL_INIT     "/MT /GR -O1 /D NDEBUG -std=c++1z -fms-extensions -fexceptions")
set(CMAKE_CXX_FLAGS_RELEASE_INIT        "/MT /GR -O3 /D NDEBUG -std=c++1z -fms-extensions -fexceptions")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO_INIT "/MT /GR -O2 /D NDEBUG -gline-tables-only -std=c++1z -fms-extensions -fexceptions")
