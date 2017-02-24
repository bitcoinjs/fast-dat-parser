all: pgblocks pgscripts

leveldb: leveldb.cpp ../include/slice.hpp ../include/hvectors.hpp
	g++ -O3 $< -I../include -I../. ../libconsensus/*.cpp -std=c++14 -o $@

pgblocks: pgblocks.cpp pg.hpp ../include/slice.hpp
	g++ -O3 $< -I../include -I../. ../libconsensus/*.cpp -std=c++14 -o $@

pgscripts: pgscripts.cpp pg.hpp ../include/hash.hpp ../include/slice.hpp
	g++ -O3 $< -I../include -I../. ../libconsensus/*.cpp -std=c++14 -o $@

clean:
	rm -f pgblocks pgscripts
