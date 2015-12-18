# scripts

``` bash
cat ~/.bitcoin/headers.dat | ./pg_import_blocks | psql
cat ~/.bitcoin/scripts.dat | ./pg_import_scripts ~/.bitcoin/headers.dat | psql
```
