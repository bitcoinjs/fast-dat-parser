#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>

#include "block.hpp"
#include "functors.hpp"
#include "hash.hpp"
#include "slice.hpp"
#include "threadpool.hpp"

int main (int argc, char** argv) {
	std::unique_ptr<processFunctor_t> delegate;
	size_t memoryAlloc = 200 * 1024 * 1024;
	size_t nThreads = 1;

	// parse command line arguments
	for (auto i = 1; i < argc; ++i) {
		const auto arg = argv[i];
		size_t functorIndex = 0;

		if (sscanf(arg, "-f%lu", &functorIndex) == 1) {
			assert(delegate == nullptr);
			if (functorIndex == 0) delegate.reset(new dumpHeaders());
			else if (functorIndex == 1) delegate.reset(new dumpScripts());
			else if (functorIndex == 2) delegate.reset(new dumpScriptIndexMap());
			else if (functorIndex == 3) delegate.reset(new dumpScriptIndex());
			else if (functorIndex == 4) delegate.reset(new dumpStatistics());
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
	FixedSlice buffer(halfMemoryAlloc);
	std::cerr << "Initialized compute buffer (" << halfMemoryAlloc << " bytes)" << std::endl;

	FixedSlice iobuffer(halfMemoryAlloc);
	std::cerr << "Initialized IO buffer (" << halfMemoryAlloc << " bytes)" << std::endl;

	ThreadPool<std::function<void(void)>> pool(nThreads);
	std::cerr << "Initialized " << nThreads << " threads in the thread pool" << std::endl;

	size_t count = 0;
	size_t remainder = 0;

	// disable buffering for stdin
	setvbuf (stdin, nullptr, _IONBF, 0);

	while (true) {
		const auto rbuf = iobuffer.drop(remainder);
		const auto read = fread(rbuf.begin, 1, rbuf.length(), stdin);
		const auto eof = static_cast<size_t>(read) < rbuf.length();

		// wait for all workers before overwrite
		pool.wait();

		// copy iobuffer to buffer, allows iobuffer to be modified independently after
		memcpy(buffer.begin, iobuffer.begin, halfMemoryAlloc);

		auto data = buffer.take(remainder + read);
		std::cerr << "-- Processed " << count << " blocks (processing " << data.length() / 1024 << " KiB)" << (eof ? " EOF" : "") << std::endl;

		while (data.length() >= 88) {
			// skip bad data (e.g bitcoind zero pre-allocations)
			if (data.peek<uint32_t>() != 0xd9b4bef9) {
				data.popFrontN(1);
				continue;
			}

			// skip bad data cont.
			const auto header = data.drop(8).take(80);
			if (!Block(header).verify()) {
				data.popFrontN(1);

				std::cerr << "--- Invalid block" << std::endl;
				continue;
			}

			// do we have enough data?
			const auto length = data.drop(4).peek<uint32_t>();
			const auto total = 8 + length;
			if (total > data.length()) break;
			data.popFrontN(8);

			// send the block data to the threadpool
			const auto block = Block(data.take(80), data.drop(80));

			pool.push([block, &delegate]() {
				delegate->operator()(block);
			});
			count++;

			data.popFrontN(length);
		}

		if (eof) break;

		// assign remainder to front of iobuffer (rbuf is offset to avoid overwrite on rawRead)
		remainder = data.length();
		memcpy(iobuffer.begin, data.begin, remainder);
	}

	// wait for all workers before delete
	pool.wait();

	return 0;
}
