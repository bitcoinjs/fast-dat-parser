all: pg_import_blocks pg_import_scripts

pg_import_blocks: pg_import_blocks.cpp
	g++ -O3 $< -I../include -I../. -std=c++14 -o $@

pg_import_scripts: pg_import_scripts.cpp
	g++ -O3 $< -I../include -I../. -std=c++14 -o $@

clean:
	rm -f pg_import_blocks pg_import_scripts
