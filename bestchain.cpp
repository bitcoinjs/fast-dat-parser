#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <vector>

#include "hash.hpp"
#include "hvectors.hpp"
#include "slice.hpp"

struct Block {
	hash256_t hash = {};
	hash256_t prevBlockHash = {};
	uint32_t bits;

	Block () {}
	Block (const hash256_t& hash, const hash256_t& prevBlockHash, const uint32_t bits) : hash(hash), prevBlockHash(prevBlockHash), bits(bits) {}
};

void printerr_hash32 (hash256_t hash) {
	std::cerr << std::hex;
	for (size_t i = 31; i < 32; --i) {
		std::cerr << std::setw(2) << std::setfill('0') << (uint32_t) hash[i];
	}
	std::cerr << std::dec;
}

template <typename F>
auto walkChain (const HMap<hash256_t, Block>& blocks, Block visitor, F f) {
	// naively walk the chain
	while (true) {
		const auto prevBlockIter = blocks.find(visitor.prevBlockHash);

		// is the visitor a genesis block? (no prevBlockIter)
		if (prevBlockIter == blocks.end()) break;
		if (f(prevBlockIter->second)) break;

		visitor = prevBlockIter->second;
	}
}

// find all blocks who are not parents to any other blocks (aka, a chain tip)
auto findChainTips (const HMap<hash256_t, Block>& blocks) {
	std::map<hash256_t, bool> hasChildren;

	for (const auto& blockIter : blocks) {
		const auto& block = blockIter.second;

		// ignore genesis block
		if (blocks.find(block.prevBlockHash) == blocks.end()) continue;

		hasChildren[block.prevBlockHash] = true;
	}

	std::vector<Block> tips;
	for (const auto& blockIter : blocks) {
		const auto& block = blockIter.second;

		// filter to only blocks who have no children
		if (hasChildren.find(block.hash) != hasChildren.end()) continue;

		tips.emplace_back(block);
	}

	return tips;
}

auto determineWork (const HMap<hash256_t, size_t>& workCache, const HMap<hash256_t, Block>& blocks, const Block block) {
	size_t totalWork = block.bits;

	walkChain(blocks, block, [&](const Block& visitor) {
		const auto prevBlockWorkCacheIter = workCache.find(visitor.prevBlockHash);
		if (prevBlockWorkCacheIter != workCache.end()) {
			totalWork += prevBlockWorkCacheIter->second;
			return true;
		}

		totalWork += visitor.bits;
		return false;
	});

	return totalWork;
}

auto findBestChain (const HMap<hash256_t, Block>& blocks) {
	auto bestBlock = Block();
	size_t bestChainWork = 0;

	HMap<hash256_t, size_t> workCache;

	for (const auto& blockIter : blocks) {
		const auto& block = blockIter.second;
		const auto chainWork = determineWork(workCache, blocks, block);

		workCache.insort(block.hash, chainWork);
		if (chainWork > bestChainWork) {
			bestBlock = block;
			bestChainWork = chainWork;
		}
	}

	std::vector<Block> blockchain;
	blockchain.push_back(bestBlock);

	walkChain(blocks, bestBlock, [&](const Block& visitor) {
		blockchain.push_back(visitor);
		return false;
	});

	std::reverse(blockchain.begin(), blockchain.end());
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

			hash256(hash.begin(), rbuf, 80);
			memcpy(prevBlockHash.begin(), rbuf + 4, 32);
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
	const auto bestBlockChain = findBestChain(blocks);

	// print some general information
	{
		const auto genesis = bestBlockChain.front();
		const auto tip = bestBlockChain.back();

		// output
		std::cerr << "Best chain" << std::endl;
		std::cerr << "- Height: " << bestBlockChain.size() - 1 << std::endl;
		std::cerr << "- Genesis: ";
		printerr_hash32(genesis.hash);
		std::cerr << std::endl << "- Tip: ";
		printerr_hash32(tip.hash);
		std::cerr << std::endl;
	}

	// output the best chain [in order]
	{
		HMap<hash256_t, uint32_t> blockChainMap;

		int32_t height = 0;
		for (const auto& block : bestBlockChain) {
			blockChainMap.push_back(std::make_pair(block.hash, height));
			++height;
		}
		blockChainMap.sort();

		StackSlice<36> buffer;
		for (auto&& blockIter : blockChainMap) {
			auto _data = buffer.drop(0);
			_data.writeN(blockIter.first.begin(), 32);
			_data.write<uint32_t>(blockIter.second);
			assert(_data.length() == 0);

			fwrite(buffer.begin(), buffer.length(), 1, stdout);
		}
	}

	return 0;
}
