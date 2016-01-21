#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <vector>

#include "hash.hpp"
#include "hvectors.hpp"

struct Block {
	hash256_t hash = {};
	hash256_t prevBlockHash = {};
	uint32_t bits;

	Block () {}
	Block (const hash256_t& hash, const hash256_t& prevBlockHash, const uint32_t bits) : hash(hash), prevBlockHash(prevBlockHash), bits(bits) {}
};

// find all blocks who are not parents to any other blocks (aka, a chain tip)
auto findChainTips(const HMap<hash256_t, Block>& blocks) {
	std::map<hash256_t, bool> parents;

	for (const auto& blockIter : blocks) {
		const auto& block = blockIter.second;
		if (blocks.find(block.prevBlockHash) == blocks.end()) continue;

		parents[block.prevBlockHash] = true;
	}

	std::vector<Block> tips;
	for (const auto& blockIter : blocks) {
		const auto& block = blockIter.second;
		if (parents.find(block.hash) != parents.end()) continue;

		tips.emplace_back(block);
	}

	return tips;
}

auto determineWork(const HMap<hash256_t, size_t>& workCache, const HMap<hash256_t, Block>& blocks, const Block source) {
	auto visitor = source;
	size_t totalWork = source.bits;

	// naively walk the chain
	while (true) {
		const auto prevBlockIter = blocks.find(visitor.prevBlockHash);
		if (prevBlockIter == blocks.end()) break;

		const auto prevBlockWorkCacheIter = workCache.find(visitor.prevBlockHash);
		if (prevBlockWorkCacheIter != workCache.end()) {
			totalWork += prevBlockWorkCacheIter->second;
			break;
		}

		visitor = prevBlockIter->second;
		totalWork += visitor.bits;
	}

	return totalWork;
}

auto findBest(const HMap<hash256_t, Block>& blocks) {
	auto bestBlock = Block();
	size_t mostWork = 0;

	HMap<hash256_t, size_t> workCache;

	for (const auto& blockIter : blocks) {
		const auto& block = blockIter.second;
		const auto work = determineWork(workCache, blocks, block);

		workCache.insort(block.hash, work);

		if (work > mostWork) {
			bestBlock = block;
			mostWork = work;
		}
	}

	auto visitor = bestBlock;
	std::vector<Block> blockchain;
	blockchain.push_back(visitor);

	// walk the chain
	while (true) {
		const auto prevBlockIter = blocks.find(visitor.prevBlockHash);
		if (prevBlockIter == blocks.end()) break;

		visitor = prevBlockIter->second;
		blockchain.push_back(visitor);
	}

	return blockchain;
}

int main () {
	HMap<hash256_t, Block> blocks;

	// read block headers from stdin until EOF
	{
		while (true) {
			uint8_t rbuf[80];
			const auto read = fread(rbuf, sizeof(rbuf), 1, stdin);

			// EOF?
			if (read == 0) break;

			hash256_t hash, prevBlockHash;
			uint32_t bits;

			hash256(&hash[0], rbuf, 80);
			memcpy(&prevBlockHash[0], rbuf + 4, 32);
			memcpy(&bits, rbuf + 72, 4);

			blocks.emplace_back(std::make_pair(hash, Block(hash, prevBlockHash, bits)));
		}

		std::cerr << "Read " << blocks.size() << " headers" << std::endl;
		blocks.sort();
		std::cerr << "Sorted " << blocks.size() << " headers" << std::endl;
	}

	// how many tips exist?
	{
		const auto chainTips = findChainTips(blocks);
		std::cerr << "Found " << chainTips.size() << " chain tips" << std::endl;
	}

	// what is the best?
	{
		const auto bestBlockChain = findBest(blocks);
		const auto genesis = bestBlockChain.back();
		const auto tip = bestBlockChain.front();

		// output
		std::cerr << "Best chain" << std::endl;
		std::cerr << "- Height: " << bestBlockChain.size() - 1 << std::endl;
		std::cerr << "- Genesis: " << std::hex;
		for (size_t i = 31; i < 32; --i) std::cerr << std::setw(2) << std::setfill('0') << (uint32_t) genesis.hash[i];
		std::cerr << std::endl << "- Tip: ";
		for (size_t i = 31; i < 32; --i) std::cerr << std::setw(2) << std::setfill('0') << (uint32_t) tip.hash[i];
		std::cerr << std::endl << std::dec;

		for (auto&& blockIter : blocks) {
			fwrite(&blockIter.first[0], 32, 1, stdout);
		}
	}

	return 0;
}
