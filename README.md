# fast-dat-parser

* Includes orphan blocks
* Skips bitcoind allocated zero-byte gaps

For fastest performance, pre-process the *.dat files to exclude orphans and remove zero-byte gaps, probably.

Parses the blockchain about as fast as your IO can pipe it out.

All memory is allocated up front.

Output goes to `stdout`, `stderr` is used for logging.


### parser

A fast `blk*.dat` parser for bitcoin blockchain analysis.

- `-f` - parse function (default `0`, see pre-packaged parse functions below)
- `-m` - memory usage (default `104857600` bytes, ~100 MiB)
- `-n` - N threads for parallel computation (default `1`)
- `-w` - whitelist file, for omitting blocks from parsing


#### parse functions

- `0` - Outputs solely the *unordered* 80-byte block headers, may include orphans (binary, not hex)
- `1` - Outputs every script hash, for every transaction, in every block, ~`BLOCK_HASH | TX_HASH | SCRIPT_HASH` (binary, not hex)

Use `-w` to avoid orphan data being included. (see example for best chain filtering)


### bestchain

A best-chain filter for block headers.
Accepts 80-byte block headers until EOF, finds the best-chain then outputs the resultant list of block hashes.


## Example

``` bash
# parse the local-best blockchain
cat ~/.bitcoin/blocks/blk*.dat | ./parser -f0 -n4 > _headers.dat
cat _headers.dat | ./bestchain > headers.dat
rm _headers.dat

# parse only blocks who's hash is found in headers.dat (from above)
cat ~/.bitcoin/blocks/blk*.dat | ./parser -f1 -n4 -wheaders.dat > scripts.dat
```
