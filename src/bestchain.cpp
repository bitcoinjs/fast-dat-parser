#include <cstring>
#include <iostream>
#include <map>
#include <vector>

#include "hash.hpp"
#include "hvectors.hpp"
#include "ranger.hpp"
#include "serial.hpp"

struct Block {
	uint256_t hash = {};
	uint256_t prevBlockHash = {};
	uint32_t bits;
	uint64_t cachedChainWork;

	Block () {}
	Block (const uint256_t& hash, const uint256_t& prevBlockHash, const uint32_t bits) : hash(hash), prevBlockHash(prevBlockHash), bits(bits), cachedChainWork(0) {}
};

template <typename F>
auto walkChain (const HVector<uint256_t, Block>& blocks, Block visitor, F f) {
	// naively walk the chain
	while (true) {
		const auto prevBlockIter = blocks.find(visitor.prevBlockHash);

		// is the visitor a genesis block? (no prevBlockIter)
		if (prevBlockIter == blocks.end()) break;
		if (f(prevBlockIter->second)) break;

		visitor = prevBlockIter->second;
	}
}

// find all blocks who have no children (chain tips)
auto findChainTips (const HVector<uint256_t, Block>& blocks) {
	std::map<uint256_t, bool> hasChildren;

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

auto determineWork (const HVector<uint256_t, Block>& blocks, const Block block) {
	uint64_t totalWork = block.bits;

	walkChain(blocks, block, [&](const Block& visitor) {
		if (visitor.cachedChainWork != 0) {
			totalWork += visitor.cachedChainWork;
			return true;
		}

		totalWork += visitor.bits;
		return false;
	});

	return totalWork;
}

auto findBestChain (HVector<uint256_t, Block>& blocks) {
	auto bestBlock = Block();
	uint64_t bestChainWork = 0;

	for (auto& blockIter : blocks) {
		auto& block = blockIter.second;
		const auto chainWork = determineWork(blocks, block);
		block.cachedChainWork = chainWork;

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
	HVector<uint256_t, Block> blocks;

	// read block headers from stdin until EOF
	{
		while (true) {
			std::array<uint8_t, 80> header;
			const auto read = fread(header.data(), header.size(), 1, stdin);

			// EOF?
			if (read == 0) break;

			const auto hash = hash256(header);
			const auto bits = serial::peek<uint32_t>(range(header).drop(72));

			uint256_t prevBlockHash;
			memcpy(prevBlockHash.begin(), header.begin() + 4, 32);

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
		std::cerr << "- Genesis: " << toHex64(genesis.hash) << std::endl;
		std::cerr << "- Tip: " << toHex64(tip.hash) << std::endl;
	}

	// output the best chain [in order]
	{
		HVector<uint256_t, uint32_t> blockChainMap;

		int32_t height = 0;
		for (const auto& block : bestBlockChain) {
			blockChainMap.push_back(std::make_pair(block.hash, height));
			++height;
		}
		blockChainMap.sort();

		std::array<uint8_t, 36> buffer;
		for (auto&& blockIter : blockChainMap) {
			auto _data = range(buffer);
			_data.put(range(blockIter.first));
			serial::put<uint32_t>(_data, blockIter.second);
			assert(_data.size() == 0);

			fwrite(buffer.begin(), buffer.size(), 1, stdout);
		}
	}

	return 0;
}
