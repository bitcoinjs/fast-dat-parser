#include <atomic>
#include <map>
#include <set>

#include "block.hpp"
#include "hash.hpp"
#include "hvectors.hpp"

struct processFunctor_t {
	virtual ~processFunctor_t () {}

	virtual bool initialize (const char*) {
		return false;
	}
	virtual void operator() (const Block&) = 0;
};

struct whitelisted_t : processFunctor_t {
	HSet<hash256_t> whitelist;

	bool initialize (const char* arg) {
		if (strncmp(arg, "-w", 2) == 0) {
			const auto fileName = std::string(arg + 2);
			const auto file = fopen(fileName.c_str(), "r");
			assert(file != nullptr);

			fseek(file, 0, SEEK_END);
			const auto fileSize = ftell(file);
			fseek(file, 0, SEEK_SET);

			assert(sizeof(this->whitelist[0]) == 32);
			assert(fileSize % sizeof(this->whitelist[0]) == 0);

			this->whitelist.resize(fileSize / sizeof(hash256_t));
			const auto read = fread(&this->whitelist[0], fileSize, 1, file);
			assert(read == 1);

			fclose(file);
			assert(!this->whitelist.empty());
			assert(std::is_sorted(this->whitelist.begin(), this->whitelist.end()));

			std::cerr << "Whitelisted " << this->whitelist.size() << " hashes" << std::endl;

			return true;
		}

		return false;
	}

	bool shouldSkip (const Block& block) const {
		if (this->whitelist.empty()) return false;

		hash256_t hash;
		hash256(&hash[0], block.header);
		return !this->whitelist.contains(hash);
	}

	bool shouldSkip (const hash256_t& hash) const {
		if (this->whitelist.empty()) return false;

		return !this->whitelist.contains(hash);
	}
};

// XXX: fwrite can be used without sizeof(sbuf) < PIPE_BUF (4096 bytes)

// BLOCK_HEADER > stdout
struct dumpHeaders : whitelisted_t {
	void operator() (const Block& block) {
		if (this->shouldSkip(block)) return;

		fwrite(block.header.begin, 80, 1, stdout);
	}
};

// SCRIPT_LENGTH | SCRIPT > stdout
struct dumpScripts : whitelisted_t {
	void operator() (const Block& block) {
		if (this->shouldSkip(block)) return;

		uint8_t sbuf[4096];
		const auto maxScriptLength = sizeof(sbuf) - sizeof(uint16_t);

		auto transactions = block.transactions();
		while (!transactions.empty()) {
			const auto& transaction = transactions.front();

			for (const auto& input : transaction.inputs) {
				if (input.script.length() > maxScriptLength) continue;
				const auto scriptLength = static_cast<uint16_t>(input.script.length());

				Slice(sbuf, sbuf + sizeof(sbuf)).put(scriptLength);
				memcpy(sbuf + sizeof(uint16_t), input.script.begin, scriptLength);
				fwrite(sbuf, sizeof(uint16_t) + scriptLength, 1, stdout);
			}

			for (const auto& output : transaction.outputs) {
				if (output.script.length() > maxScriptLength) continue;
				const auto scriptLength = static_cast<uint16_t>(output.script.length());

				Slice(sbuf, sbuf + sizeof(sbuf)).put(scriptLength);
				memcpy(sbuf + sizeof(uint16_t), output.script.begin, scriptLength);
				fwrite(sbuf, sizeof(uint16_t) + scriptLength, 1, stdout);
			}

			transactions.popFront();
		}
	}
};

// PREV_TX_HASH | PREV_TX_VOUT | TX_HASH > stdout
struct dumpTxOutIndex : whitelisted_t {
	void operator() (const Block& block) {
		if (this->shouldSkip(block)) return;

		uint8_t sbuf[68];

		auto transactions = block.transactions();
		while (!transactions.empty()) {
			const auto& transaction = transactions.front();

			hash256(sbuf + 36, transaction.data);

			for (const auto& input : transaction.inputs) {
				memcpy(sbuf, input.hash.begin, 32);
				Slice(sbuf + 32, sbuf + 36).put(input.vout);

				fwrite(sbuf, sizeof(sbuf), 1, stdout);
			}

			transactions.popFront();
		}
	}
};

struct dumpStatistics : whitelisted_t {
	std::atomic_ulong inputs;
	std::atomic_ulong outputs;
	std::atomic_ulong transactions;

	~dumpStatistics () {
		std::cout << this->inputs << '\n' << this->outputs << '\n' << this->transactions << std::endl;
	}

	void operator() (const Block& block) {
		if (this->shouldSkip(block)) return;

		auto transactions = block.transactions();
		this->transactions += transactions.length();

		while (!transactions.empty()) {
			const auto& transaction = transactions.front();

			this->inputs += transaction.inputs.size();
			this->outputs += transaction.outputs.size();

			transactions.popFront();
		}
	}
};

struct dumpOtherStatistics : whitelisted_t {
	std::atomic_ulong custom;
	std::atomic_ulong transactions;
	std::atomic_ulong blocks;

	~dumpOtherStatistics () {
		std::cout <<
			"nLockTimes != 0:\t" << this->custom << '\n' <<
			"Transactions:\t" << this->transactions << '\n' <<
			"Blocks:\t" << this->blocks << std::endl;
	}

	void operator() (const Block& block) {
		if (this->shouldSkip(block)) return;

		this->blocks++;
		auto transactions = block.transactions();
		this->transactions += transactions.length();

		while (!transactions.empty()) {
			const auto& transaction = transactions.front();

			this->custom += transaction.locktime != 0;
			transactions.popFront();
		}
	}
};

struct dumpVersionStatistics : whitelisted_t {
	std::atomic_ulong v1;
	std::atomic_ulong v2;
	std::atomic_ulong transactions;
	std::atomic_ulong blocks;

	~dumpVersionStatistics () {
		std::cout << this->v1 << '\n' << this->v2 << '\n' << this->transactions << '\n' << this->blocks << std::endl;
	}

	void operator() (const Block& block) {
		if (this->shouldSkip(block)) return;

		this->blocks++;
		auto transactions = block.transactions();
		this->transactions += transactions.length();

		while (!transactions.empty()) {
			const auto& transaction = transactions.front();

			this->v1 += transaction.version == 1;
			this->v2 += transaction.version == 2;

			transactions.popFront();
		}
	}
};

// SHA1(TX_HASH | VOUT) | SHA1(OUTPUT_SCRIPT) > stdout
struct dumpScriptIndexMap : whitelisted_t {
	void operator() (const Block& block) {
		if (this->shouldSkip(block)) return;

		uint8_t sbuf[40] = {};
		uint8_t tbuf[36] = {};

		auto transactions = block.transactions();
		while (!transactions.empty()) {
			const auto& transaction = transactions.front();

			hash256(tbuf, transaction.data);

			uint32_t vout = 0;
			for (const auto& output : transaction.outputs) {
				Slice(tbuf + 32, tbuf + sizeof(tbuf)).put(vout);
				++vout;

				sha1(sbuf, tbuf, sizeof(tbuf));
				sha1(sbuf + 20, output.script);

				fwrite(sbuf, sizeof(sbuf), 1, stdout);
			}

			transactions.popFront();
		}
	}
};

const uint8_t COINBASE[32] = {};

// BLOCK_HASH | TX_HASH | SHA1(PREVIOUS_OUTPUT_SCRIPT) > stdout
// BLOCK_HASH | TX_HASH | SHA1(OUTPUT_SCRIPT) > stdout
struct dumpScriptIndex : whitelisted_t {
	HMap<hash160_t, hash160_t> txOuts;

	bool initialize (const char* arg) {
		if (whitelisted_t::initialize(arg)) return true;
		if (strncmp(arg, "-i", 2) == 0) {
			const auto fileName = std::string(arg + 2);
			const auto file = fopen(fileName.c_str(), "r");
			assert(file != nullptr);

			fseek(file, 0, SEEK_END);
			const auto fileSize = ftell(file);
			fseek(file, 0, SEEK_SET);

			assert(sizeof(this->txOuts[0]) == 40);
			assert(fileSize % sizeof(this->txOuts[0]) == 0);

			this->txOuts.resize(fileSize / sizeof(this->txOuts[0]));
			const auto read = fread(&this->txOuts[0], fileSize, 1, file);
			assert(read == 1);

			fclose(file);
			assert(!this->txOuts.empty());
			assert(std::is_sorted(this->txOuts.begin(), this->txOuts.end()));

			std::cerr << "Mapped " << this->txOuts.size() << " txOuts" << std::endl;

			return true;
		}

		return false;
	}

	void operator() (const Block& block) {
		hash256_t hash;
		hash256(&hash[0], block.header);
		if (this->shouldSkip(hash)) return;

		uint8_t sbuf[84] = {};
		memcpy(sbuf, &hash[0], 32);

		auto transactions = block.transactions();
		while (!transactions.empty()) {
			const auto& transaction = transactions.front();

			hash256(sbuf + 32, transaction.data);

			if (!this->txOuts.empty()) {
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
