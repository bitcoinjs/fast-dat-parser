all: bestchain parser

bestchain: bestchain.cpp
	g++ -O3 $< -Iinclude -I. libconsensus/*.cpp -std=c++14 -o $@

parser: parser.cpp
	g++ -pthread -O3 $< -Iinclude -I. libconsensus/*.cpp -std=c++14 -o $@

clean:
	rm -f bestchain parser
