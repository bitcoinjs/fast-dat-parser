# fast-dat-parser

* Includes orphan blocks
* Skips bitcoind allocated zero-byte gaps

For fastest performance, pre-process the *.dat files to exclude orphans and remove zero-byte gaps, probably.

Parses the blockchain about as fast as your IO can pipe it out.  For a typical SSD, this can be around ~450 MiB/s.

All memory is allocated up front.

Output goes to `stdout`, `stderr` is used for logging.


### parser

A fast `blk*.dat` parser for bitcoin blockchain analysis.

- `-f` - parse function (default `0`, see pre-packaged parse functions below)
- `-j` - N threads for parallel computation (default `1`)
- `-m` - memory usage (default `104857600` bytes, ~100 MiB)
- `-w` - whitelist file, for omitting blocks from parsing


#### parse functions

Each of these functions write their output as raw data (binary, not hex) to `stdout`.

- `0` - Output the *unordered* 80-byte block headers, includes orphans
- `1` - Outputs every script prefixed with a `uint16_t` length

Use a whitelist (see `-w`) to avoid orphan data being included. (see below examples for filtering by best chain)


### bestchain

A best-chain filter for block headers.
Accepts 80-byte block headers until EOF, finds the best-chain then outputs the resultant list of block hashes.


## Examples

``` bash
# parse the local-best blockchain
cat ~/.bitcoin/blocks/blk*.dat | ./parser -f0 -j4 | ./bestchain > ~/.bitcoin/headers.dat

# parse only blocks who's hash is found in headers.dat (from above)
cat ~/.bitcoin/blocks/blk*.dat | ./parser -f1 -j4 -wheaders.dat > ~/.bitcoin/scripts.dat
```
