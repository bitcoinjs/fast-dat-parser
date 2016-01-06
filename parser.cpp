#include <array>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>
#include <set>

#include "block.hpp"
#include "hash.hpp"
#include "slice.hpp"
#include "threadpool.hpp"

// XXX: no locking for fwrite if sizeof(wbuf) < PIPE_BUF (4096 bytes)

// BLOCK_HEADER
void dumpHeaders (Slice data) {
	fwrite(data.begin, 80, 1, stdout);
}

// SCRIPT_LENGTH | SCRIPT
void dumpScripts (Slice data) {
	const auto block = Block(data.take(80), data.drop(80));
	uint8_t wbuf[4096];

	auto transactions = block.transactions();
	while (!transactions.empty()) {
		const auto& transaction = transactions.front();

		for (const auto& input : transaction.inputs) {
			if (input.script.length() > sizeof(wbuf) - sizeof(uint16_t)) continue;
			const auto scriptLength = static_cast<uint16_t>(input.script.length());

			Slice(wbuf, wbuf + sizeof(uint16_t)).put<uint16_t>(scriptLength);
			memcpy(wbuf + sizeof(uint16_t), input.script.begin, scriptLength);
			fwrite(wbuf, sizeof(uint16_t) + scriptLength, 1, stdout);
		}

		for (const auto& output : transaction.outputs) {
			if (output.script.length() > sizeof(wbuf) - sizeof(uint16_t)) continue;
			const auto scriptLength = static_cast<uint16_t>(output.script.length());

			Slice(wbuf, wbuf + sizeof(uint16_t)).put<uint16_t>(scriptLength);
			memcpy(wbuf + sizeof(uint16_t), output.script.begin, scriptLength);
			fwrite(wbuf, sizeof(uint16_t) + scriptLength, 1, stdout);
		}

		transactions.popFront();
	}
}

typedef void(*processFunction_t)(Slice);
processFunction_t FUNCTIONS[] = {
	&dumpHeaders,
	&dumpScripts
};

auto importWhitelist (const std::string& fileName) {
	std::set<hash256_t> set;

	auto file = fopen(fileName.c_str(), "r");
	if (file == nullptr) return set;

	while (true) {
		hash256_t hash;
		const auto read = fread(&hash[0], 32, 1, file);

		// EOF?
		if (read == 0) break;

		set.emplace(hash);
	}

	return set;
}

static size_t memoryAlloc = 100 * 1024 * 1024;
static size_t nThreads = 1;
static size_t functionIndex = 0;
static std::string whitelistFileName;

auto parseArg (char* argv) {
	if (sscanf(argv, "-f%lu", &functionIndex) == 1) return true;
	if (sscanf(argv, "-j%lu", &nThreads) == 1) return true;
	if (sscanf(argv, "-m%lu", &memoryAlloc) == 1) return true;
	if (strncmp(argv, "-w", 2) == 0) {
		whitelistFileName = std::string(argv + 2);
		return true;
	}

	return false;
}

int main (int argc, char** argv) {
	for (auto i = 1; i < argc; ++i) {
		if (!parseArg(argv[i])) return 1;
	}

	const auto delegate = FUNCTIONS[functionIndex];

	// if specified, import the whitelist
	const auto doWhitelist = !whitelistFileName.empty();
	std::set<hash256_t> whitelist;
	if (doWhitelist) {
		whitelist = importWhitelist(whitelistFileName);
		if (whitelist.empty()) return 1;

		std::cerr << "Initialized whitelist (" << whitelist.size() << " entries)" << std::endl;
	}

	// pre-allocate back buffer
	const auto halfMemoryAlloc = memoryAlloc / 2;
	auto _buffer = new uint8_t[halfMemoryAlloc];
	Slice buffer(_buffer, _buffer + halfMemoryAlloc);
	std::cerr << "Initialized compute buffer (" << halfMemoryAlloc << " bytes)" << std::endl;

	auto _iobuffer = new uint8_t[halfMemoryAlloc];
	Slice iobuffer(_iobuffer, _iobuffer + halfMemoryAlloc);
	std::cerr << "Initialized IO buffer (" << halfMemoryAlloc << " bytes)" << std::endl;

	ThreadPool<std::function<void(void)>> pool(nThreads);
	std::cerr << "Initialized " << nThreads << " threads in the thread pool" << std::endl;

	uint64_t remainder = 0;
	size_t count = 0;

	while (true) {
		const auto rbuf = iobuffer.drop(remainder);
		const auto read = fread(rbuf.begin, 1, rbuf.length(), stdin);
		const auto eof = static_cast<size_t>(read) < rbuf.length();

		// wait for all workers before overwrite
		pool.wait();

		// swap the buffers (maintains memory safety across threads)
		std::swap(buffer, iobuffer);

		auto data = buffer.take(remainder + read);
		std::cerr << "-- " << count << " Blocks (processing " << data.length() / 1024 << " KiB)" << (eof ? " EOF" : "") << std::endl;

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
				continue;
			}

			// do we have enough data?
			const auto length = data.drop(4).peek<uint32_t>();
			const auto total = 8 + length;
			if (total > data.length()) break;

			// is whitelisting in effect?
			if (doWhitelist) {
				hash256_t hash;
				hash256(&hash[0], header);

				// skip if not found
				if (whitelist.find(hash) == whitelist.end()) {
					data.popFrontN(total);

					std::cerr << "--- Skipped block" << std::endl;
					continue;
				}
			}

			// lets do it, send the block data to the threadpool
			const auto block = data.drop(8).take(length);

			pool.push([block, delegate]() { delegate(block); });
			count++;

			data.popFrontN(total);
		}

		if (eof) break;

		// assign remainder to front of iobuffer (rbuf is offset to avoid overwrite on rawRead)
		remainder = data.length();
		memcpy(iobuffer.begin, data.begin, remainder);
	}

	delete[] _buffer;
	delete[] _iobuffer;

	return 0;
}
