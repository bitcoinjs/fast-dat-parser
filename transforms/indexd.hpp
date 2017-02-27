#pragma once

#include "hvectors.hpp"
#include "base.hpp"

// SHA256(SCRIPT) | HEIGHT | TX_HASH | VOUT > stdout
struct dumpScriptIndex : transform_t {
	void operator() (const Block& block) {
		uint32_t height = -1;
		if (this->shouldSkip(block, nullptr, &height)) return;

		uint8_t sbuf[72];
		Slice(sbuf + 32, sbuf + 36).put(height);

		auto transactions = block.transactions();
		while (not transactions.empty()) {
			const auto& transaction = transactions.front();

			hash256(sbuf + 36, transaction.data);

			uint32_t vout = 0;
			for (const auto& output : transaction.outputs) {
				hash256(sbuf, output.script);
				Slice(sbuf + 68, sbuf + 72).put(vout);
				++vout;

				fwrite(sbuf, sizeof(sbuf), 1, stdout);
			}

			transactions.popFront();
		}
	}
};

// PREV_TX_HASH | PREV_TX_VOUT | TX_HASH | TX_VIN > stdout
struct dumpSpentIndex : transform_t {
	void operator() (const Block& block) {
		if (this->shouldSkip(block)) return;

		uint8_t sbuf[72];

		auto transactions = block.transactions();
		while (not transactions.empty()) {
			const auto& transaction = transactions.front();

			hash256(sbuf + 36, transaction.data);

			uint32_t vin = 0;
			for (const auto& input : transaction.inputs) {
				memcpy(sbuf, input.hash.begin, 32);
				Slice(sbuf + 32, sbuf + 36).put(input.vout);
				Slice(sbuf + 68, sbuf + 72).put(vin);
				++vin;

				fwrite(sbuf, sizeof(sbuf), 1, stdout);
			}

			transactions.popFront();
		}
	}
};

// TX_HASH | HEIGHT > stdout
struct dumpTxIndex : transform_t {
	void operator() (const Block& block) {
		uint32_t height = -1;
		if (this->shouldSkip(block, nullptr, &height)) return;

		uint8_t sbuf[36];
		Slice(sbuf + 32, sbuf + 36).put(height);

		auto transactions = block.transactions();
		while (not transactions.empty()) {
			const auto& transaction = transactions.front();

			hash256(sbuf, transaction.data);

			transactions.popFront();
		}
	}
};

// TX_HASH | VOUT | VALUE > stdout
struct dumpTxOutIndex : transform_t {
	void operator() (const Block& block) {
		if (this->shouldSkip(block)) return;

		uint8_t sbuf[44];

		auto transactions = block.transactions();
		while (not transactions.empty()) {
			const auto& transaction = transactions.front();

			hash256(sbuf, transaction.data);

			uint32_t vout = 0;
			for (const auto& output : transaction.outputs) {
				Slice(sbuf + 32, sbuf + 36).put(vout);
				Slice(sbuf + 36, sbuf + 44).put(output.value);
				++vout;

				fwrite(sbuf, sizeof(sbuf), 1, stdout);
			}

			transactions.popFront();
		}
	}
};
