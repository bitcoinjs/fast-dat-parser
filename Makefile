CXX=g++
CFLAGS=-pedantic -std=c++1z -W -Wall -Wcast-qual -Wconversion -Werror -Wextra -Wwrite-strings
#OFLAGS=-O3 -ggdb3
OFLAGS=-O3
LFLAGS=-lcrypto
IFLAGS=-Iinclude

SOURCES=$(shell find src/ -name '*.c' -o -name '*.cpp')
OBJECTS=$(addsuffix .o, $(basename $(SOURCES)))
DEPENDENCIES=$(OBJECTS:.o=.d)

# TARGETS
.PHONY: all clean deps

all: bestchain parser

clean:
	$(RM) $(DEPENDENCIES) $(OBJECTS) bestchain parser

bestchain: $(filter-out src/parser.o, $(OBJECTS))
	$(CXX) $(filter-out src/parser.o, $(OBJECTS)) $(LFLAGS) $(OFLAGS) -o $@

parser: $(filter-out src/bestchain.o, $(OBJECTS))
	$(CXX) $(filter-out src/bestchain.o, $(OBJECTS)) $(LFLAGS) $(OFLAGS) -pthread -o $@

# INFERENCES
%.o: %.cpp
	$(CXX) $(CFLAGS) $(OFLAGS) $(IFLAGS) -MMD -MP -c $< -o $@

deps:
	curl 'https://raw.githubusercontent.com/dcousens/hexxer/d76f9e526676535fd4c2584a8f84582994c35996/hexxer.h' > include/hexxer.hpp
	curl 'https://raw.githubusercontent.com/dcousens/ranger/aa3b7712b4d080df420e342eef416a65de3d5eaf/ranger.hpp' > include/ranger.hpp
	curl 'https://raw.githubusercontent.com/dcousens/ranger/aa3b7712b4d080df420e342eef416a65de3d5eaf/serial.hpp' > include/serial.hpp

-include $(DEPENDENCIES)
