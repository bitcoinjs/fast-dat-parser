CXX=g++
CFLAGS=-pedantic -std=c++1z -W -Wall -Wcast-qual -Wconversion -Werror -Wextra -Wwrite-strings
OFLAGS=-O3
LFLAGS=-lcrypto
IFLAGS=-Iinclude -I.

all: bestchain parser

bestchain: bestchain.cpp include/hash.hpp include/hvectors.hpp include/slice.hpp
	$(CXX) $(CFLAGS) $(LFLAGS) $(OFLAGS) $(IFLAGS) $< -o $@

parser: parser.cpp include/block.hpp include/hash.hpp include/hvectors.hpp include/threadpool.hpp include/transforms.hpp transforms/*.hpp include/slice.hpp
	$(CXX) $(CFLAGS) $(LFLAGS) $(OFLAGS) $(IFLAGS) -lleveldb -pthread $< -o $@

include/slice.hpp:
	curl https://raw.githubusercontent.com/dcousens/ranger/master/slice.hpp > include/slice.hpp

clean:
	rm -f bestchain parser
