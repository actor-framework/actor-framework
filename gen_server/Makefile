CXX = /opt/local/bin/g++-mp-4.6
CXXFLAGS = -std=c++0x -pedantic -Wall -Wextra -I/opt/local/include/ -I/Users/neverlord/libcppa -fpermissive -O2
LIBS = -L/opt/local/lib -lboost_thread-mt  -L/Users/neverlord/libcppa/.libs/ -lcppa

all:
	$(CXX) $(CXXFLAGS) $(LIBS) -o sequential_send sequential_send.cpp
	$(CXX) $(CXXFLAGS) $(LIBS) -o parallel_send parallel_send.cpp
