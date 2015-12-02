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

// BLOCK_HEADER
void dumpHeaders (Slice<uint8_t> data) {
	fwrite(data.begin, 80, 1, stdout);
}

// BLOCK_HASH | TX_HASH | SCRIPT_HASH
void dumpScriptShas (Slice<uint8_t> data) {
	const auto block = Block(data.take(80), data.drop(80));
	uint8_t wbuf[32 + 32 + 20] = {};

	hash256(&wbuf[0], block.header);

	auto transactions = block.transactions();
	while (!transactions.empty()) {
		const auto& transaction = transactions.front();

		hash256(&wbuf[32], transaction.data);

		for (const auto& input : transaction.inputs) {
			sha1(&wbuf[64], input.script);

			// no locking, 84 bytes < PIPE_BUF (4096 bytes)
			fwrite(wbuf, sizeof(wbuf), 1, stdout);
		}

		for (const auto& output : transaction.outputs) {
			sha1(&wbuf[64], output.script);

			// no locking, 84 bytes < PIPE_BUF (4096 bytes)
			fwrite(wbuf, sizeof(wbuf), 1, stdout);
		}

		transactions.popFront();
	}
}

typedef void(*processFunction_t)(Slice<uint8_t>);
processFunction_t FUNCTIONS[] = {
	&dumpHeaders,
	&dumpScriptShas
};

typedef std::array<uint8_t, 32> hash_t;

auto importWhitelist (const std::string& fileName) {
	std::set<hash_t> set;

	auto file = fopen(fileName.c_str(), "r");
	if (file == nullptr) return set;

	do {
		hash_t hash;
		const auto read = fread(&hash[0], 32, 1, file);

		// EOF?
		if (read == 0) break;

		set.emplace(hash);
	} while (true);

	return set;
}

static size_t memoryAlloc = 100 * 1024 * 1024;
static size_t nThreads = 1;
static size_t functionIndex = 0;
static std::string whitelistFileName;

auto parseArg (char* argv) {
	if (sscanf(argv, "-f%lu", &functionIndex) == 1) return true;
	if (sscanf(argv, "-m%lu", &memoryAlloc) == 1) return true;
	if (sscanf(argv, "-n%lu", &nThreads) == 1) return true;
	if (strncmp(argv, "-w", 2) == 0) {
		whitelistFileName = std::string(&argv[2]);
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
	std::set<hash_t> whitelist;
	if (doWhitelist) {
		whitelist = importWhitelist(whitelistFileName);
		if (whitelist.empty()) return 1;

		std::cerr << "Initialized whitelist (" << whitelist.size() << " entries)" << std::endl;
	}

	// pre-allocate buffers
	std::vector<uint8_t> backbuffer(memoryAlloc);
	auto iobuffer = Slice<uint8_t>(&backbuffer[0], &backbuffer[0] + memoryAlloc / 2);
	auto buffer = Slice<uint8_t>(&backbuffer[0] + memoryAlloc / 2, &backbuffer[0] + memoryAlloc);
	ThreadPool<std::function<void(void)>> pool(nThreads);

	std::cerr << "Initialized buffers (2 * " << memoryAlloc / 2 << " bytes)" << std::endl;
	std::cerr << "Initialized " << nThreads << " threads in the thread pool" << std::endl;

	uint64_t remainder = 0;
	bool dirty = false;
	size_t count = 0;

	while (true) {
		const auto rdbuf = iobuffer.drop(remainder);
		const auto read = fread(rdbuf.begin, 1, rdbuf.length(), stdin);
		const auto eof = static_cast<size_t>(read) < rdbuf.length();

		// wait for all workers before overwrite
		pool.wait();

		// swap the buffers
		std::swap(buffer, iobuffer);

		auto slice = buffer.take(remainder + read);
		std::cerr << "-- " << count << " Blocks (processing " << slice.length() / 1024 << " KiB)" << std::endl;

		while (slice.length() >= 88) {
			// skip bad data (e.g bitcoind zero pre-allocations)
			if (slice.peek<uint32_t>() != 0xd9b4bef9) {
				slice.popFrontN(4);
				dirty = true;

				continue;
			}

			// skip bad data cont. (verify if dirty)
			const auto header = slice.drop(8).take(80);
			if (dirty) {
				if (!Block(header).verify()) {
					slice.popFrontN(4);

					continue;
				}

				dirty = false;
			}

			// do we have enough data?
			const auto length = slice.drop(4).peek<uint32_t>();
			const auto total = 8 + length;
			if (total > slice.length()) break;

			// is whitelisting in effect?
			if (doWhitelist) {
				hash_t hash;
				hash256(&hash[0], &header[0], 80);

				// skip if not found
				if (whitelist.find(hash) == whitelist.end()) {
					slice.popFrontN(total);

					std::cerr << "--- Skipped block" << std::endl;
					continue;
				}
			}

			// lets do it, send the data to the threadpool
			const auto data = slice.drop(8).take(length);

			pool.push([data, delegate]() { delegate(data); });
			count++;

			slice.popFrontN(total);
		}

		if (eof) break;

		// assign remainder to front of iobuffer (rdbuf is offset to avoid overwrite on rawRead)
		remainder = slice.length();
		memcpy(&iobuffer[0], &slice[0], remainder);
	}

	return 0;
}
