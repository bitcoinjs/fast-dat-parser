#include <array>
#include <cassert>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <vector>

#include "hash.hpp"

typedef std::array<uint8_t, 32> hash_t;

struct Block {
	hash_t hash = {};
	hash_t prevBlockHash = {};
	uint32_t bits;

	Block () {}
	Block (const hash_t& hash, const hash_t& prevBlockHash, const uint32_t bits) : hash(hash), prevBlockHash(prevBlockHash), bits(bits) {}
};

// find all blocks who are not parents to any other blocks (aka, a chain tip)
auto findChainTips(const std::map<hash_t, Block>& blocks) {
	std::map<hash_t, bool> parents;

	for (auto& blockIter : blocks) {
		const auto block = blockIter.second;
		if (blocks.count(block.prevBlockHash) == 0) continue;

		parents[block.prevBlockHash] = true;
	}

	std::vector<Block> tips;
	for (auto& blockIter : blocks) {
		const auto block = blockIter.second;
		if (parents.find(block.hash) != parents.end()) continue;

		tips.push_back(block);
	}

	return tips;
}

auto determineWork(std::map<hash_t, size_t>& workCache, const std::map<hash_t, Block>& blocks, const Block source) {
	// maybe already done?
	const auto workPair = workCache.find(source.hash);
	if (workPair != workCache.end()) return workPair->second;

	// nope, shit
	auto visitor = source;
	std::vector<Block> todo;
	todo.push_back(visitor);

	// walk the chain until a pre-calculated ancestor or the root is found
	while (true) {
		const auto visitorPrevWorkIter = workCache.find(visitor.prevBlockHash);
		if (visitorPrevWorkIter != workCache.end()) break;

		const auto visitorPrevBlockIter = blocks.find(visitor.prevBlockHash);

		// is this visitor a genesis block? (no previous block)
		if (visitorPrevBlockIter == blocks.end()) break;

		visitor = visitorPrevBlockIter->second;
		todo.push_back(visitor);
	}

	size_t chainWork = 0;

	// iterated in reverse, genesis blocks will appear first
	for (auto it = todo.rbegin(); it != todo.rend(); ++it) {
		const auto todoBlock = *it;
		const auto todoBlockWork = static_cast<size_t>(todoBlock.bits);
		const auto todoPrevBlockWorkIter = workCache.find(todoBlock.prevBlockHash);

		// is this todo block a genesis block? (no previous block)
		if (todoPrevBlockWorkIter == workCache.end()) {
			chainWork += todoBlockWork;
			workCache.emplace(todoBlock.hash, todoBlockWork);
			continue;
		}

		const auto todoPrevBlockWork = todoPrevBlockWorkIter->second;
		chainWork += todoPrevBlockWork + todoBlockWork;
		workCache.emplace(todoBlock.hash, todoPrevBlockWork + todoBlockWork);
	}

	return chainWork;
}

auto findBest(const std::map<hash_t, Block>& blocks) {
	auto bestBlock = Block();
	size_t mostWork = 0;

	std::cerr << "fB" << std::endl;

	std::map<hash_t, size_t> workCache;

	for (auto& blockIter : blocks) {
		const auto block = blockIter.second;
		const auto work = determineWork(workCache, blocks, block);

		std::cerr << "fBcmp" << std::endl;

		if (work > mostWork) {
			bestBlock = block;
			mostWork = work;
		}
	}

	std::cerr << "fBE" << std::endl;

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
	std::map<hash_t, Block> blocks;

	{
		do {
			uint8_t buffer[80];
			const auto read = fread(&buffer[0], sizeof(buffer), 1, stdin);

			// EOF?
			if (read == 0) break;

			hash_t hash, prevBlockHash;
			uint32_t bits;

			hash256(&hash[0], &buffer[0], 80);
			memcpy(&prevBlockHash[0], &buffer[4], 32);
			memcpy(&bits, &buffer[72], 4);

			blocks[hash] = Block(hash, prevBlockHash, bits);
		} while (true);

		std::cerr << "Read " << blocks.size() << " headers" << std::endl;
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
		std::cerr << std::hex;
		std::cerr << "- Genesis: ";
		for (size_t i = 31; i < 32; --i) std::cerr << std::setw(2) << std::setfill('0') << (uint32_t) genesis.hash[i];
		std::cerr << std::endl << "- Tip: ";
		for (size_t i = 31; i < 32; --i) std::cerr << std::setw(2) << std::setfill('0') << (uint32_t) tip.hash[i];
		std::cerr << std::endl << std::dec;

		for (auto it = bestBlockChain.rbegin(); it != bestBlockChain.rend(); ++it) {
			fwrite(&it->hash[0], 32, 1, stdout);
		}
	}

	return 0;
}
