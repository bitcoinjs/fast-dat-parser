#pragma once

#include <atomic>
#include <mutex>
#include <vector>
#include "transforms.hpp"
using namespace ranger;

// HEIGHT | VALUE > stdout
template <typename Block>
struct dumpOutputValuesOverHeight : public TransformBase<Block> {
	void operator() (const Block& block) {
		uint32_t height = 0xffffffff;
		if (this->shouldSkip(block, nullptr, &height)) return;

		std::array<uint8_t, 12> buffer;
		serial::place<uint32_t>(buffer, height);

		auto transactions = block.transactions();
		while (not transactions.empty()) {
			const auto& transaction = transactions.front();

			for (const auto& output : transaction.outputs) {
				serial::place<uint64_t>(range(buffer).drop(4), output.value);
				fwrite(buffer.begin(), buffer.size(), 1, stdout);
			}

			transactions = transactions.drop();
		}
	}
};

auto perc (uint64_t a, uint64_t ab) {
	return static_cast<double>(a) / static_cast<double>(ab);
}

template <typename Block>
struct dumpStatistics : public TransformBase<Block> {
	std::atomic_ulong inputs;
	std::atomic_ulong outputs;
	std::atomic_ulong transactions;
	std::atomic_ulong version1;
	std::atomic_ulong version2;
	std::atomic_ulong locktimesGt0;
	std::atomic_ulong nonFinalSequences;

	dumpStatistics () {
		this->inputs = 0;
		this->outputs = 0;
		this->transactions = 0;
		this->version1 = 0;
		this->version2 = 0;
		this->locktimesGt0 = 0;
		this->nonFinalSequences = 0;
	}

	virtual ~dumpStatistics () {
		std::cout <<
			"Transactions:\t" << this->transactions << '\n' <<
			"-- Inputs:\t" << this->inputs << " (ratio " << perc(this->inputs, this->transactions) << ") \n" <<
			"-- Outputs:\t" << this->outputs << " (ratio " << perc(this->outputs, this->transactions) << ") \n" <<
			"-- Version1:\t" << this->version1 << " (" << perc(this->version1, this->transactions) * 100 << "%) \n" <<
			"-- Version2:\t" << this->version2 << " (" << perc(this->version2, this->transactions) * 100 << "%) \n" <<
			"-- Locktimes (>0):\t" << this->locktimesGt0 << " (" << perc(this->locktimesGt0, this->transactions) * 100 << "%) \n" <<
			"-- Sequences (!= FINAL):\t" << this->nonFinalSequences << " (" << perc(this->nonFinalSequences, this->inputs) * 100 << "%) \n" <<
			std::endl;
	}

	void operator() (const Block& block) {
		if (this->shouldSkip(block)) return;

		auto transactions = block.transactions();
		this->transactions += transactions.size();

		while (not transactions.empty()) {
			const auto& transaction = transactions.front();

			this->inputs += transaction.inputs.size();

			size_t nfs = 0;
			for (const auto& input : transaction.inputs) {
				if (input.sequence != 0xffffffff) nfs++;
			}

			this->nonFinalSequences += nfs;
			this->outputs += transaction.outputs.size();

			this->version1 += transaction.version == 1;
			this->version2 += transaction.version == 2;
			this->locktimesGt0 += transaction.locktime > 0;

			transactions = transactions.drop();
		}
	}
};

// ASM > stdout
template <typename Block>
struct dumpASM : public TransformBase<Block> {
	void operator() (const Block& block) {
		if (this->shouldSkip(block)) return;

		std::array<uint8_t, 1024*1024> buffer;

		auto transactions = block.transactions();
		while (not transactions.empty()) {
			const auto& transaction = transactions.front();

			for (const auto& output : transaction.inputs) {
				auto tmp = range(buffer);
				putASM(tmp, output.script);
				serial::put<char>(tmp, '\n');
				const auto lineLength = buffer.size() - tmp.size();

				// FIXME: stdout is non-atomic past 4096
				if (lineLength > 4096) continue;

				fwrite(buffer.begin(), lineLength, 1, stdout);
			}

			transactions = transactions.drop();
		}
	}
};

// BLOCK_HEADER > stdout
template <typename Block>
struct dumpHeaders : public TransformBase<Block> {
	void operator() (const Block& block) {
		if (this->shouldSkip(block)) return;

		fwrite(block.header.begin(), 80, 1, stdout);
	}
};

// SCRIPT_LENGTH | SCRIPT > stdout
template <typename Block>
struct dumpScripts : public TransformBase<Block> {
	void operator() (const Block& block) {
		if (this->shouldSkip(block)) return;

		std::array<uint8_t, 4096> buffer;
		const auto maxScriptLength = buffer.size() - sizeof(uint16_t);

		auto transactions = block.transactions();
		while (not transactions.empty()) {
			const auto& transaction = transactions.front();

			for (const auto& input : transaction.inputs) {
				if (input.script.size() > maxScriptLength) continue;

				auto r = range(buffer);
				serial::put<uint16_t>(r, static_cast<uint16_t>(input.script.size()));
				r.put(input.script);
				fwrite(buffer.begin(), buffer.size() - r.size(), 1, stdout);
			}

			for (const auto& output : transaction.outputs) {
				if (output.script.size() > maxScriptLength) continue;

				auto r = range(buffer);
				serial::put<uint16_t>(r, static_cast<uint16_t>(output.script.size()));
				r.put(output.script);
				fwrite(buffer.begin(), buffer.size() - r.size(), 1, stdout);
			}

			transactions = transactions.drop();
		}
	}
};

typedef std::pair<std::vector<uint8_t>, uint64_t> TxoDetail;
typedef std::pair<uint256_t, uint32_t> Txin;
typedef std::pair<Txin, TxoDetail> Txo;

// HEIGHT | VALUE > stdout
template <typename Block>
struct dumpUnspents : public TransformBase<Block> {
	static constexpr auto BLANK_TXIN = Txin{ {}, 0 };

	std::mutex mutex;
	HList<Txin, TxoDetail> unspents;

	void operator() (const Block& block) {
		if (this->shouldSkip(block)) return;

		std::vector<Txin> txins;
		std::vector<Txo> txos;

		auto transactions = block.transactions();
		while (not transactions.empty()) {
			const auto& transaction = transactions.front();
			const auto txHash = transaction.hash();

			for (const auto& input : transaction.inputs) {
				uint256_t prevTxHash;
				std::copy(input.hash.begin(), input.hash.end(), prevTxHash.begin());

				txins.emplace_back(Txin{prevTxHash, input.vout});
			}

			uint32_t vout = 0;
			for (const auto& output : transaction.outputs) {
				std::vector<uint8_t> script;
				script.resize(output.script.size());
				std::copy(output.script.begin(), output.script.end(), script.begin());

				txos.emplace_back(Txo({txHash, vout}, {script, output.value}));
				++vout;
			}

			transactions = transactions.drop();
		}

		std::lock_guard<std::mutex>(this->mutex);
		for (const auto& txo : txos) {
			this->unspents.insort(txo.first, txo.second);
		}

		for (const auto& txin : txins) {
			const auto iter = this->unspents.find(txin);
			if (iter == this->unspents.end()) continue; // uh, maybe you are only doing part of the blockchain!

			iter->first = BLANK_TXIN;
		}

		this->unspents.erase(std::remove_if(
			this->unspents.begin(),
			this->unspents.end(),
			[](const auto& x) {
				return x.first == BLANK_TXIN;
			}
		), this->unspents.end());

		std::cout << this->unspents.size() << std::endl;
	}
};
