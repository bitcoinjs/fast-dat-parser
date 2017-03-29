CXX=g++
CFLAGS=-pedantic -std=c++1z -W -Wall -Wcast-qual -Wconversion -Werror -Wextra -Wwrite-strings
OFLAGS=-O3
LFLAGS=-lcrypto
IFLAGS=-Iinclude

SOURCES=$(shell find src/ -name '*.c' -o -name '*.cpp')
OBJECTS=$(addsuffix .o, $(basename $(SOURCES)))
DEPENDENCIES=$(OBJECTS:.o=.d)

# TARGETS
.PHONY: all clean

all: bestchain parser

clean:
	$(RM) $(DEPENDENCIES) $(OBJECTS) demo1

bestchain: $(filter-out src/parser.o, $(OBJECTS))
	$(CXX) $(filter-out src/parser.o, $(OBJECTS)) $(LFLAGS) $(OFLAGS) -o $@

parser: $(filter-out src/bestchain.o, $(OBJECTS))
	$(CXX) $(filter-out src/bestchain.o, $(OBJECTS)) $(LFLAGS) $(OFLAGS) -lleveldb -pthread -o $@

# INFERENCES
%.o: %.cpp
	$(CXX) $(CFLAGS) $(OFLAGS) $(IFLAGS) -MMD -MP -c $< -o $@

-include $(DEPENDENCIES)
