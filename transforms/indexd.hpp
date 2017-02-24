#pragma once

#include "hvectors.hpp"
#include "transform_base.hpp"

// XXX: fwrite can be used without sizeof(sbuf) < PIPE_BUF (4096 bytes)
const uint8_t COINBASE[32] = {};

// 0x06 BLOCK_HASH | TX_HASH | SHA1(OUTPUT_SCRIPT) > stdout
struct dumpScriptIndex : transform_t {
	HMap<hash160_t, hash160_t> txOuts;

	void operator() (const Block& block) {
		hash256_t hash;
		hash256(&hash[0], block.header);
		if (this->shouldSkip(hash)) return;

		uint8_t sbuf[84] = {};
		memcpy(sbuf, &hash[0], 32);

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
						sha1(&hash[0], input.script);
						memcpy(sbuf + 64, &hash[0], 20);
						fwrite(sbuf, sizeof(sbuf), 1, stdout);

						continue;
					}

					hash160_t hash;
					sha1(&hash[0], input.data.take(36));

					const auto txOutIter = this->txOuts.find(hash);
					assert(txOutIter != this->txOuts.end());

					memcpy(sbuf + 64, &(txOutIter->second)[0], 20);
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
