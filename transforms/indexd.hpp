#pragma once

#include "hvectors.hpp"
#include "base.hpp"

// XXX: fwrite can be used without sizeof(sbuf) < PIPE_BUF (4096 bytes)
const uint8_t COINBASE[32] = {};

// SHA256(SCRIPT) | HEIGHT | TX_HASH | VOUT > stdout
struct dumpScriptIndex : transform_t {
	void operator() (const Block& block) {
		hash256_t hash;
		hash256(hash.begin(), block.header);
		if (this->shouldSkip(hash)) return;

		uint8_t sbuf[84] = {};
		memcpy(sbuf, hash.begin(), 32);

		auto transactions = block.transactions();
		while (not transactions.empty()) {
			const auto& transaction = transactions.front();

			hash256(sbuf + 32, transaction.data);

			if (not this->txOuts.empty()) {
				for (const auto& input : transaction.inputs) {
					// Coinbase input?
					if (
						input.vout == 0xffffffff &&
						memcmp(input.hash.begin, COINBASE, sizeof(COINBASE)) == 0
					) {
						sha1(hash.begin(), input.script);
						memcpy(sbuf + 64, hash.begin(), 20);
						fwrite(sbuf, sizeof(sbuf), 1, stdout);

						continue;
					}

					hash160_t hash;
					sha1(hash.begin(), input.data.take(36));

					const auto txOutIter = this->txOuts.find(hash);
					assert(txOutIter != this->txOuts.end());

					memcpy(sbuf + 64, txOutIter->second.begin(), 20);
					fwrite(sbuf, sizeof(sbuf), 1, stdout);
				}
			}

			for (const auto& output : transaction.outputs) {
				sha1(sbuf + 64, output.script);
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

				fwrite(sbuf, sizeof(sbuf), 1, stdout);
				++vin;
			}

			transactions.popFront();
		}
	}
};

// HEIGHT | TX_HASH | TX_VIN > stdout
struct dumpTxIndex : transform_t {
	void operator() (const Block& block) {
		if (this->shouldSkip(block)) return;

		uint8_t sbuf[40];
		Slice(sbuf, sbuf + 4).put(block);

		auto transactions = block.transactions();
		while (not transactions.empty()) {
			const auto& transaction = transactions.front();

			hash256(sbuf + 4, transaction.data);

			uint32_t vout = 0;
			for (const auto& output : transaction.outputs) {
				Slice(sbuf + 36, sbuf + 44).put(output.value);

				fwrite(sbuf, sizeof(sbuf), 1, stdout);
				++vout;
			}

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

				fwrite(sbuf, sizeof(sbuf), 1, stdout);
				++vout;
			}

			transactions.popFront();
		}
	}
};
