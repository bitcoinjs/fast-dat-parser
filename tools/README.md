# scripts

``` bash
cat ~/.bitcoin/headers.dat | ./pgblocks | node ./pgcopy.js
cat ~/.bitcoin/scripts.dat | ./pgscripts ~/.bitcoin/headers.dat | node ./pgcopy.js
```
