#pragma once

#include <atomic>
#include "transform_base.hpp"

// HEIGHT | VALUE > stdout
struct dumpOutputValuesOverHeight : transform_t {
	void operator() (const Block& block) {
		if (this->shouldSkip(block)) return;

		uint8_t sbuf[8] = {};
// 		Slice(sbuf, sbuf + sizeof(sbuf)).put(block.utc());

		auto transactions = block.transactions();
		while (not transactions.empty()) {
			const auto& transaction = transactions.front();

			for (const auto& output : transaction.outputs) {
				Slice(sbuf, sbuf + sizeof(sbuf)).put(output.value);

				fwrite(sbuf, sizeof(sbuf), 1, stdout);
			}

			transactions.popFront();
		}
	}
};

auto perc (uint64_t a, uint64_t ab) {
	return static_cast<double>(a) / static_cast<double>(ab);
}

struct dumpStatistics : transform_t {
	std::atomic_ulong inputs;
	std::atomic_ulong outputs;
	std::atomic_ulong transactions;
	std::atomic_ulong version1;
	std::atomic_ulong version2;
	std::atomic_ulong locktimesGt0;

	dumpStatistics () {
		this->inputs = 0;
		this->outputs = 0;
		this->transactions = 0;
		this->version1 = 0;
		this->version2 = 0;
		this->locktimesGt0 = 0;
	}

	~dumpStatistics () {
		std::cout <<
			"Transactions:\t" << this->transactions << '\n' <<
			"-- Inputs:\t" << this->inputs << " (ratio " << perc(this->inputs, this->transactions) << ") \n" <<
			"-- Outputs:\t" << this->outputs << " (ratio " << perc(this->outputs, this->transactions) << ") \n" <<
			"-- Version1:\t" << this->version1 << " (" << perc(this->version1, this->transactions) * 100 << "%) \n" <<
			"-- Version2:\t" << this->version2 << " (" << perc(this->version2, this->transactions) * 100 << "%) \n" <<
			"-- Locktimes (>0):\t" << this->locktimesGt0 << " (" << perc(this->locktimesGt0, this->transactions) * 100 << "%) \n" <<
			std::endl;
	}

	void operator() (const Block& block) {
		if (this->shouldSkip(block)) return;

		auto transactions = block.transactions();
		this->transactions += transactions.length();

		while (not transactions.empty()) {
			const auto& transaction = transactions.front();

			this->inputs += transaction.inputs.size();
			this->outputs += transaction.outputs.size();

			this->version1 += transaction.version == 1;
			this->version2 += transaction.version == 2;
			this->locktimesGt0 += transaction.locktime > 0;

			transactions.popFront();
		}
	}
};
