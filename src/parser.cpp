#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <functional>
#include <iostream>
#include <memory>

#include "bitcoin.hpp"
#include "hash.hpp"
#include "ranger.hpp"
#include "serial.hpp"
#include "threadpool.hpp"

#include "raw.hpp"
#include "statistics.hpp"
#include "unspents.hpp"
// #include "leveldb.hpp"

using backing_vector_t = std::vector<uint8_t>;
using block_t = decltype(Block(ptr_range(backing_vector_t()), ptr_range(backing_vector_t())));
using thread_function_t = std::function<void(void)>;
using transform_function_t = std::function<void(block_t)>;

int main (int argc, char** argv) {
	size_t memoryAlloc = 200 * 1024 * 1024;
	size_t nThreads = 1;

	std::unique_ptr<TransformBase<block_t>> delegate;

	// parse command line arguments
	for (auto i = 1; i < argc; ++i) {
		const auto arg = argv[i];
		size_t transformIndex = 0;

		if (sscanf(arg, "-t%lu", &transformIndex) == 1) {
			assert(delegate == nullptr);

			// raw
			if (transformIndex == 0) delegate.reset(new dumpHeaders<block_t>());
			else if (transformIndex == 1) delegate.reset(new dumpScripts<block_t>());

			// statistics
			else if (transformIndex == 2) delegate.reset(new dumpStatistics<block_t>());
			else if (transformIndex == 3) delegate.reset(new dumpOutputValuesOverHeight<block_t>());
			else if (transformIndex == 4) delegate.reset(new dumpUnspents<block_t>());

			// indexd
// 			else if (transformIndex == 4) delegate.reset(new dumpLeveldb<block_t>());

			continue;
		}
		if (sscanf(arg, "-j%lu", &nThreads) == 1) continue;
		if (sscanf(arg, "-m%lu", &memoryAlloc) == 1) continue;

		if (delegate && delegate->initialize(arg)) continue;
		assert(false);
	}

	time_t start, end;
	time(&start);

	// pre-allocate buffers
	const auto halfMemoryAlloc = memoryAlloc / 2;
	backing_vector_t iobuffer(halfMemoryAlloc);
	backing_vector_t parsebuffer(halfMemoryAlloc);
	std::cerr << "Allocated IO buffer (" << halfMemoryAlloc << " bytes)" << std::endl;
	std::cerr << "Allocated parse buffer (" << halfMemoryAlloc << " bytes)" << std::endl;

	ThreadPool<thread_function_t> pool(nThreads);
	std::cerr << "Initialized " << nThreads << " threads in the thread pool" << std::endl;

	size_t count = 0;
	size_t remainder = 0;
	size_t accum = 0;
	size_t invalid = 0;

	while (true) {
		const auto available = iobuffer.size() - remainder;
		const auto read = std::fread(iobuffer.data() + remainder, 1, available, stdin);
		const auto eof = static_cast<size_t>(read) < available;
		accum += read;

		// wait for all workers before overwrite
		pool.wait();

		// copy [all of] iobuffer to parsebuffer, releasing iobuffer for the next read
		std::copy(iobuffer.cbegin(), iobuffer.cend(), parsebuffer.begin());

		auto data = ptr_range(parsebuffer).take(remainder + read);
		std::cerr << "-- Parsed "
			<< count << " blocks ("
			<< "read " << read / 1024 << " KiB, "
			<< accum / 1024 / 1024 << " MiB total, "
			<< "skipped " << invalid / 1024 << "KiB)"
			<< (eof ? " EOF" : "")
			<< std::endl;

		while (data.size() >= 88) {
			// skip bad data (e.g bitcoind zero pre-allocations)
			if (serial::peek<uint32_t>(data) != 0xd9b4bef9) {
				data.popFrontN(1);
				continue;
			}

			// skip bad data cont.
			const auto header = data.drop(8).take(80);
			if (not Block(header, header.drop(80)).verify()) {
				data.popFrontN(1);
				++invalid;
				continue;
			}

			// do we have enough data?
			const auto length = serial::peek<uint32_t>(data.drop(4));
			const auto total = 8 + length;
			if (total > data.size()) break;
			data.popFrontN(8);

			// send the block data to the threadpool
			const auto block = Block(header, data.drop(80));
			pool.push([block, &delegate]() {
				delegate->operator()(block);
			});

			count++;

			data.popFrontN(length);
		}

		if (eof) break;

		// assign remainder to front of iobuffer (w/ next read offset by remainder)
		std::copy(data.begin(), data.end(), iobuffer.begin());
		remainder = data.size();
	}

	time(&end);
	std::cerr << "Parsed "
		<< count << " blocks ("
		<< accum / 1024 / 1024 << " MiB)"
		<< "in " << difftime(end, start) << " seconds"
		<< std::endl;

	return 0;
}
