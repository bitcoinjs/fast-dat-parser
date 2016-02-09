# fast-dat-parser

* Includes orphan blocks
* Skips bitcoind allocated zero-byte gaps

For fastest performance, pre-process the *.dat files to exclude orphans and remove zero-byte gaps, probably.

Parses the blockchain about as fast as your IO can pipe it out.  For a typical SSD, this can be around ~450 MiB/s.

All memory is allocated up front.

Output goes to `stdout`, `stderr` is used for logging.


### parser

A fast `blk*.dat` parser for bitcoin blockchain analysis.

- `-f<FUNCTION INDEX>` - parse function (default `0`, see pre-packaged parse functions below)
- `-j<THREADS>` - N threads for parallel computation (default `1`)
- `-m<BYTES>` - memory usage (default `209715200` bytes, ~200 MiB)
- `-w<FILENAME>` - whitelist file, for omitting blocks from parsing


#### parse functions (`-f`)

Each of these pre-included functions write their output as raw data (binary, not hex).

- `0` - Output the *unordered* 80-byte block headers, includes orphans
- `1` - Outputs every script prefixed with a `uint16_t` length
- `2` - Outputs a txOut dump of `SHA1(TX_HASH | VOUT) | SHA1(OUTPUT_SCRIPT)`'s
- `3` - Outputs a script index of `BLOCK_HASH | TX_HASH | SHA1(OUTPUT_SCRIPT)`, if a `txOutMap` file is specified via `-txo<FILENAME>`, `BLOCK_HASH | TX_HASH | SHA1(PREVIOUS_OUTPUT_SCRIPT)`'s are also written for each transaction input.

Use a whitelist (see `-w`) to avoid orphan data being included. (see below examples for filtering by best chain)
You can easily write your own though!

### bestchain

A best-chain filter for block headers.
Accepts 80-byte block headers until EOF, finds the best-chain then outputs the resultant list of block hashes.


## Examples

**Output all scripts for the local-best blockchain**
``` bash
# parse the local-best blockchain
cat ~/.bitcoin/blocks/blk*.dat | ./parser -f0 | ./bestchain > headers.dat

# output every script found in the local-best blockchain
cat ~/.bitcoin/blocks/blk*.dat | ./parser -j4 -f1 -wheaders.dat > ~/.bitcoin/scripts.dat
```

**Output a script index for the local-best blockchain**
``` bash
# parse the local-best blockchain
cat ~/.bitcoin/blocks/blk*.dat | ./parser -f0 | ./bestchain > headers.dat

# output a txOut index
cat ~/.bitcoin/blocks/blk*.dat | ./parser -j4 -f2 -wheaders.dat > txoindex.dat

# output a script index for the local-best blockchain
cat ~/.bitcoin/blocks/blk*.dat | ./parser -j4 -f3 -wheaders.dat -itxoindex.dat > ~/.bitcoin/scripts.dat
```
