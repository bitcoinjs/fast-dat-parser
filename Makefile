W=-pedantic -std=c++14 -W -Wall -Wconversion -Wextra -Wfatal-errors -Wwrite-strings -Wno-unused-variable -Wno-unused-parameter

all: binsort bestchain parser

binsort: binsort.cpp
	g++ $W -fopenmp -O3 $< -Iinclude -I. -std=c++14 -o $@

bestchain: bestchain.cpp include/hash.hpp include/hvectors.hpp
	g++ $W -O3 $< -Iinclude -I. libconsensus/*.cpp -std=c++14 -o $@

parser: parser.cpp functors.hpp include/block.hpp include/hash.hpp include/hvectors.hpp include/slice.hpp include/threadpool.hpp
	g++ $W -pthread -O3 $< -Iinclude -I. libconsensus/*.cpp -std=c++14 -o $@

clean:
	rm -f bestchain parser
