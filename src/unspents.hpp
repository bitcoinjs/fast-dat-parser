#pragma once

#include <mutex>
#include <vector>
#include "transforms.hpp"

typedef std::pair<uint256_t, uint32_t> UnspentKey;
typedef std::pair<std::vector<uint8_t>, uint64_t> UnspentValue;
typedef std::pair<UnspentKey, UnspentValue> Unspent;

// HEIGHT | VALUE > stdout
template <typename Block>
struct dumpUnspents : public TransformBase<Block> {
	std::mutex mutex;
	HMap<UnspentKey, UnspentValue> unspents;

	void operator() (const Block& block) {
		if (this->shouldSkip(block)) return;

		std::vector<UnspentKey> spends;
		std::vector<Unspent> newUnspents;

		auto transactions = block.transactions();
		while (not transactions.empty()) {
			const auto& transaction = transactions.front();
			const auto txHash = transaction.hash();

			for (const auto& input : transaction.inputs) {
				uint256_t prevTxHash;
				std::copy(input.hash.begin(), input.hash.end(), prevTxHash.begin());

				spends.emplace_back(UnspentKey{prevTxHash, input.vout});
			}

			uint32_t vout = 0;
			for (const auto& output : transaction.outputs) {
				std::vector<uint8_t> script;
				script.resize(output.script.size());
				std::copy(output.script.begin(), output.script.end(), script.begin());

				newUnspents.emplace_back(Unspent({txHash, vout}, {script, output.value}));
				++vout;
			}

			transactions.popFront();
		}

		std::lock_guard<std::mutex>(this->mutex);
		this->unspents.reserve(this->unspents.size() + newUnspents.size());
		for (const auto& unspent : newUnspents) {
			this->unspents.insort(unspent.first, unspent.second); // TODO
		}
		this->unspents.sort();

		for (const auto& spend : spends) {
			const auto iter = this->unspents.find(spend);
			if (iter == this->unspents.end()) continue;

			this->unspents.erase(iter); // TODO
		}

		std::cout << this->unspents.size() << std::endl;
	}
};
