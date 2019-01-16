# fast-dat-parser
Parses the blockchain about as fast as your IO can pipe it out.  For a typical SSD, this can be around ~450 MiB/s.

All memory is allocated up front.

Output goes to `stdout`, `stderr` is used for logging.


## Usage
A fast `blk*.dat` parser for bitcoin blockchain analysis.

- `-j<THREADS>` - N threads for parallel computation (default `1`)
- `-m<BYTES>` - memory usage (default `209715200` bytes, ~200 MiB)
- `-t<INDEX>` - transform function (default `0`, see pre-packaged transforms below)
- `-w<FILENAME>` - whitelist file, for omitting blocks from parsing

Important to note is that the implementation skips bitcoind allocated zero-byte gaps,  and includes orphan blocks unless `-w` omits them.


### Transforms (`-t`)
Each of these pre-included functions write their output as raw data (binary, not hex).
You can easily write your own though!

- `0` - Outputs the *unordered* 80-byte block headers
- `1` - Outputs every script prefixed with a `uint16_t` length
- `2` - Displays the number of transaction inputs, outputs and number of transactions in the blockchain
- `3` - Outputs `HEIGHT | VALUE` for each output,  typically used for showing output balances over time

Use a whitelist (see `-w`) to stop orphan blocks from being parsed. (see below for filtering by best chain)


## Examples
**Output all scripts for the local-best blockchain**
``` bash
# parse the local-best blockchain
cat ~/.bitcoin/blocks/blk*.dat | ./parser -t0 | ./bestchain > chain.dat

# output every script found in the local-best blockchain
cat ~/.bitcoin/blocks/blk*.dat | ./parser -j4 -t1 -wchain.dat > ~/.bitcoin/scripts.dat
```


### Useful tools
These tools are for the CLI, but will aid in preparing/using data produced by the above.


#### `bestchain`
A best-chain filter for block headers.

Accepts 80-byte block headers until EOF, then finds the best-chain in the set,  and outputs the best-chain in the form of a sorted hash map [(see HMap<K, V>)](https://github.com/dcousens/fast-dat-parser/blob/master/include/hvectors.hpp).


#### External
* [binsort](https://github.com/dcousens/binsort)
* [hexxer](https://github.com/dcousens/hexxer)
* [ranger](https://github.com/dcousens/ranger)


## LICENSE [MIT](LICENSE)
The constants and `getOpString` function in `include/bitcoin-ops.hpp` is copied from https://github.com/bitcoin/bitcoin/.
