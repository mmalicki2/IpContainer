CXXFLAGS=-g -O0 -std=c++11

all: main
main: main.cpp IpContainer.cpp IpContainer.hpp ChunkAllocator.hpp

clean:
	rm  main
