#CXX = /opt/local/bin/g++-mp-4.6
CXX = /opt/local/bin/g++-mp-4.6
CXXFLAGS = -std=c++0x -pedantic -Wall -Wextra -I/opt/local/include/ -I../ -fpermissive
LIBS = -L../.libs/ -lcppa -L/opt/local/lib -L/usr/lib -lboost_thread-mt

all:
	$(CXX) $(CXXFLAGS) -o sequential_send sequential_send.cpp $(LIBS)
#	$(CXX) $(CXXFLAGS) -o parallel_send parallel_send.cpp $(LIBS)
