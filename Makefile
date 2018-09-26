CXX=g++
CFLAGS=-pedantic -std=c++1z -W -Wall -Wcast-qual -Wconversion -Werror -Wextra -Wwrite-strings -Wno-unused-function
#OFLAGS=-O3 -ggdb3
OFLAGS=-O3
LFLAGS=-lcrypto
IFLAGS=-Iinclude

SOURCES=$(shell find src -name '*.c' -o -name '*.cpp')
OBJECTS=$(addsuffix .o, $(basename $(SOURCES)))
DEPENDENCIES=$(OBJECTS:.o=.d)
INCLUDES=include/hexxer.hpp include/ranger.hpp include/serial.hpp include/threadpool.hpp

# TARGETS
.PHONY: all clean includes

all: $(INCLUDES) bestchain parser

clean:
	$(RM) $(DEPENDENCIES) $(OBJECTS) bestchain parser

bestchain: $(filter-out src/parser.o, $(OBJECTS))
	$(CXX) $(filter-out src/parser.o, $(OBJECTS)) $(LFLAGS) $(OFLAGS) -o $@

parser: $(filter-out src/bestchain.o, $(OBJECTS))
	$(CXX) $(filter-out src/bestchain.o, $(OBJECTS)) $(LFLAGS) $(OFLAGS) -pthread -o $@

# INFERENCES
%.o: %.cpp
	$(CXX) $(CFLAGS) $(OFLAGS) $(IFLAGS) -MMD -MP -c $< -o $@

include/hexxer.hpp:
	curl 'https://raw.githubusercontent.com/dcousens/hexxer/d76f9e526676535fd4c2584a8f84582994c35996/hexxer.h' > $@

include/ranger.hpp:
	curl 'https://raw.githubusercontent.com/dcousens/ranger/568e116e931aefa437019d846fa1d36f79098679/ranger.hpp' > $@

include/serial.hpp:
	curl 'https://raw.githubusercontent.com/dcousens/ranger/568e116e931aefa437019d846fa1d36f79098679/serial.hpp' > $@

include/threadpool.hpp:
	curl 'https://raw.githubusercontent.com/dcousens/threadpool/b6f29f27b4b658f4b0df976c9151a0f76aa86335/threadpool.hpp' > $@

-include $(DEPENDENCIES)
