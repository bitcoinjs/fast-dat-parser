CXX=g++
CFLAGS=-pedantic -std=c++1z -W -Wall -Wcast-qual -Wconversion -Werror -Wextra -Wwrite-strings
OFLAGS=-O3
LFLAGS=-lcrypto
IFLAGS=-Iinclude -I.

all: bestchain parser

bestchain: bestchain.cpp include/hash.hpp include/hvectors.hpp include/ranger.hpp
	$(CXX) $(CFLAGS) $(LFLAGS) $(OFLAGS) $(IFLAGS) $< -o $@

parser: parser.cpp include/block.hpp include/hash.hpp include/hvectors.hpp include/threadpool.hpp include/transforms.hpp transforms/*.hpp include/ranger.hpp
	$(CXX) $(CFLAGS) $(LFLAGS) $(OFLAGS) $(IFLAGS) -lleveldb -pthread $< -o $@

include/ranger.hpp:
	curl https://raw.githubusercontent.com/dcousens/ranger/master/ranger.hpp > include/ranger.hpp
	curl https://raw.githubusercontent.com/dcousens/ranger/master/serial.hpp > include/serial.hpp

clean:
	rm -f bestchain parser
