#include <atomic>
#include <map>
#include <set>

#include "block.hpp"
#include "hash.hpp"

struct processFunctor_t {
	virtual ~processFunctor_t () {}

	virtual bool initialize (const char*) {
		return false;
	}
	virtual void operator() (const Block&) = 0;
};

struct whitelisted_t : processFunctor_t {
	std::vector<hash256_t> whitelist;

	bool initialize (const char* arg) {
		if (strncmp(arg, "-w", 2) == 0) {
			const auto fileName = std::string(arg + 2);
			const auto file = fopen(fileName.c_str(), "r");
			assert(file != nullptr);

			fseek(file, 0, SEEK_END);
			const auto fileSize = ftell(file);
			fseek(file, 0, SEEK_SET);

			assert(sizeof(hash256_t) == 32);
			this->whitelist.resize(fileSize / sizeof(hash256_t));
			const auto read = fread(&this->whitelist[0], fileSize, 1, file);
			assert(read == 1);
			assert(!this->whitelist.empty());

			fclose(file);

			std::cerr << "Initialized " << this->whitelist.size() << " block hashes" << std::endl;
			std::sort(this->whitelist.begin(), this->whitelist.end());
			std::cerr << "Sorted " << this->whitelist.size() << " block hashes" << std::endl;

			return true;
		}

		return false;
	}

	bool shouldSkip (const Block& block) const {
		if (this->whitelist.empty()) return false;
		hash256_t hash;
		hash256(&hash[0], block.header);

		const auto hashIter = std::lower_bound(this->whitelist.begin(), this->whitelist.end(), hash);
		return !((hashIter != this->whitelist.end()) && !(hash < *hashIter));
	}

	bool shouldSkip (const hash256_t& hash) const {
		if (this->whitelist.empty()) return false;

		const auto hashIter = std::lower_bound(this->whitelist.begin(), this->whitelist.end(), hash);
		return !((hashIter != this->whitelist.end()) && !(hash < *hashIter));
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
	typedef std::pair<hash160_t, hash160_t> TxOut;
	std::vector<TxOut> txOuts;

	bool initialize (const char* arg) {
		if (whitelisted_t::initialize(arg)) return true;
		if (strncmp(arg, "-i", 2) == 0) {
			const auto fileName = std::string(arg + 2);
			const auto file = fopen(fileName.c_str(), "r");
			assert(file != nullptr);

			fseek(file, 0, SEEK_END);
			const auto fileSize = ftell(file);
			fseek(file, 0, SEEK_SET);

			assert(sizeof(TxOut) == 40);
			this->txOuts.resize(fileSize / sizeof(TxOut));
			const auto read = fread(&this->txOuts[0], fileSize, 1, file);
			assert(read == 1);
			assert(!this->txOuts.empty());

			fclose(file);

			std::cerr << "Initialized " << this->txOuts.size() << " txOuts" << std::endl;
			std::sort(this->txOuts.begin(), this->txOuts.end(), [](const auto& a, const auto& b) {
				return a.first < b.first;
			});

			std::cerr << "Sorted " << this->txOuts.size() << " txOuts" << std::endl;

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

					const auto txOutsIter = std::lower_bound(this->txOuts.begin(), this->txOuts.end(), hash, [](const auto& a, const hash160_t& b) {
						return a.first < b;
					});
					assert((txOutsIter != this->txOuts.end()) && !(hash < txOutsIter->first));

					memcpy(sbuf + 64, &(txOutsIter->second)[0], 20);
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
