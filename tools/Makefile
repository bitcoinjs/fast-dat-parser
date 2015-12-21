all: pgcopy_blocks pgcopy_scripts

pgcopy_blocks: pgcopy_blocks.cpp
	g++ -O3 $< -I../include -I../. -std=c++14 -o $@

pgcopy_scripts: pgcopy_scripts.cpp
	g++ -O3 $< -I../include -I../. -std=c++14 -o $@

clean:
	rm -f pgcopy_blocks pgcopy_scripts
