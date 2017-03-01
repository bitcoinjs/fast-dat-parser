#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <memory>

#include "block.hpp"
#include "hash.hpp"
#include "slice.hpp"
#include "threadpool.hpp"
#include "transforms.hpp"

int main (int argc, char** argv) {
	time_t start, end;
	time(&start);

	std::unique_ptr<transform_t> delegate;
	size_t memoryAlloc = 200 * 1024 * 1024;
	size_t nThreads = 1;

	// parse command line arguments
	for (auto i = 1; i < argc; ++i) {
		const auto arg = argv[i];
		size_t transformIndex = 0;

		if (sscanf(arg, "-t%lu", &transformIndex) == 1) {
			assert(delegate == nullptr);

			// raw
			if (transformIndex == 0) delegate.reset(new dumpHeaders());
			else if (transformIndex == 1) delegate.reset(new dumpScripts());

			// indexd
			else if (transformIndex == 2) delegate.reset(new dumpScriptIndex());
			else if (transformIndex == 3) delegate.reset(new dumpSpentIndex());
			else if (transformIndex == 4) delegate.reset(new dumpTxIndex());
			else if (transformIndex == 5) delegate.reset(new dumpTxOutIndex());
			else if (transformIndex == 8) delegate.reset(new dumpIndexdLevel());

			// statistics
			else if (transformIndex == 6) delegate.reset(new dumpStatistics());
			else if (transformIndex == 7) delegate.reset(new dumpOutputValuesOverHeight());

			continue;
		}
		if (sscanf(arg, "-j%lu", &nThreads) == 1) continue;
		if (sscanf(arg, "-m%lu", &memoryAlloc) == 1) continue;

		if (delegate && delegate->initialize(arg)) continue;
		assert(false);
	}

	assert(delegate != nullptr);

	// pre-allocate buffers
	const auto halfMemoryAlloc = memoryAlloc / 2;
	HeapSlice iobuffer(halfMemoryAlloc);
	std::cerr << "Allocated IO buffer (" << halfMemoryAlloc << " bytes)" << std::endl;

	HeapSlice buffer(halfMemoryAlloc);
	std::cerr << "Allocated active buffer (" << halfMemoryAlloc << " bytes)" << std::endl;

	ThreadPool<std::function<void(void)>> pool(nThreads);
	std::cerr << "Initialized " << nThreads << " threads in the thread pool" << std::endl;

	size_t count = 0;
	size_t remainder = 0;
	size_t accum = 0;
	size_t invalid = 0;

	while (true) {
		const auto rbuf = iobuffer.drop(remainder);
		const auto read = fread(rbuf.begin(), 1, rbuf.length(), stdin);
		const auto eof = static_cast<size_t>(read) < rbuf.length();
		accum += read;

		// wait for all workers before overwrite
		pool.wait();

		// copy iobuffer to buffer, allows iobuffer to be modified independently after
		memcpy(buffer.begin(), iobuffer.begin(), halfMemoryAlloc);

		auto data = buffer.take(remainder + read);
		if (invalid > 0) {
			std::cerr << std::endl << "-- Skipped " << invalid << " bad bytes" << std::endl;
			invalid = 0;
		}
		std::cerr << "-- Parsed " << count << " blocks (read " << read / 1024 << " KiB, " << accum / 1024 / 1024 << " MiB total)" << (eof ? " EOF" : "") << std::endl;

		while (data.length() >= 88) {
			// skip bad data (e.g bitcoind zero pre-allocations)
			if (data.peek<uint32_t>() != 0xd9b4bef9) {
				data.popFrontN(1);
				continue;
			}

			// skip bad data cont.
			const auto header = data.drop(8).take(80);
			if (not Block(header).verify()) {
				data.popFrontN(1);

				std::cerr << '.';
				++invalid;
				continue;
			}

			if (invalid > 0) {
				std::cerr << std::endl << "-- Skipped " << invalid << " bad bytes" << std::endl;
				invalid = 0;
			}

			// do we have enough data?
			const auto length = data.drop(4).peek<uint32_t>();
			const auto total = 8 + length;
			if (total > data.length()) break;
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

		// assign remainder to front of iobuffer (rbuf is offset to avoid overwrite on rawRead)
		remainder = data.length();
		memcpy(iobuffer.begin(), data.begin(), remainder);
	}

	if (invalid > 0) {
		std::cerr << std::endl << "-- Skipped " << invalid << " bad bytes" << std::endl;
		invalid = 0;
	}

	time(&end);
	std::cerr << "Parsed " << count << " blocks in " << difftime(end, start) << " seconds" << std::endl;

	return 0;
}
