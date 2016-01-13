#include <map>

#include "block.hpp"
#include "hash.hpp"

struct processFunctor_t {
	virtual bool initialize(const char*) {
		return false;
	}
	virtual void operator()(const Block&) const = 0;
};

struct whitelisted_t : processFunctor_t {
	std::set<hash256_t> whitelist;

	bool initialize (const char* arg) {
		if (strncmp(arg, "-w", 2) == 0) {
			const auto fileName = std::string(arg + 2);
			const auto file = fopen(fileName.c_str(), "r");
			assert(file != nullptr);

			while (true) {
				hash256_t hash;
				const auto read = fread(&hash[0], 32, 1, file);

				// EOF?
				if (read == 0) break;

				this->whitelist.emplace(hash);
			}

			fclose(file);
			assert(!whitelist.empty());

			std::cerr << "Initialized whitelist (" << whitelist.size() << " entries)" << std::endl;

			return true;
		}

		return false;
	}

	bool skip (const Block& block) const {
		if (this->whitelist.empty()) return false;
		hash256_t hash;
		hash256(&hash[0], block.header);

		return this->whitelist.find(hash) != this->whitelist.end();
	}

	bool skipHash (const hash256_t& hash) const {
		if (this->whitelist.empty()) return false;

		return this->whitelist.find(hash) != this->whitelist.end();
	}
};

// XXX: fwrite can be used without sizeof(sbuf) < PIPE_BUF (4096 bytes)

// BLOCK_HEADER > stdoud
struct dumpHeaders : whitelisted_t {
	void operator()(const Block& block) const {
		if (this->skip(block)) return;

		fwrite(block.header.begin, 80, 1, stdout);
	}
};

// SCRIPT_LENGTH | SCRIPT > stdout
struct dumpScripts : whitelisted_t {
	void operator()(const Block& block) const {
		if (this->skip(block)) return;

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

// BLOCK_HASH | TX_HASH | OUTPUT_SCRIPT_HASH > stdout
// H(TX_HASH | VOUT) | OUTPUT_SCRIPT_HASH > file
struct dumpScriptIndexOutputs : whitelisted_t {
	FILE* txOutMapFile;

	bool initialize(const char* arg) {
		if (whitelisted_t::initialize(arg)) return true;
		if (strncmp(arg, "-fm", 3) == 0) {
			assert(!txOutMapFile);
			auto fileName = std::string(arg + 3);

			this->txOutMapFile = fopen(fileName.c_str(), "w");
			assert(this->txOutMapFile != nullptr);

			return true;
		}

		return false;
	}

	void operator()(const Block& block) const {
		hash256_t hash;
		hash256(&hash[0], block.header);
		if (this->skipHash(hash)) return;

		uint8_t sbuf[84] = {};
		uint8_t fbuf[40] = {};
		memcpy(sbuf, &hash[0], 32);

		auto transactions = block.transactions();
		while (!transactions.empty()) {
			const auto& transaction = transactions.front();

			uint8_t txOut[36];
			hash256(sbuf + 32, transaction.data);
			memcpy(txOut, sbuf + 32, 32);

			uint32_t vout = 0;
			for (const auto& output : transaction.outputs) {
				sha1(sbuf + 64, output.script);
				fwrite(sbuf, sizeof(sbuf), 1, stdout);

				if (!this->txOutMapFile) continue;

				Slice(txOut + 32, txOut + sizeof(txOut)).put(vout);
				sha1(fbuf, txOut, sizeof(txOut));
				memcpy(fbuf + 20, sbuf + 64, 20);
				fwrite(fbuf, sizeof(fbuf), 1, this->txOutMapFile);
				++vout;
			}

			transactions.popFront();
		}
	}
};

const uint8_t COINBASE[32] = {};

struct dumpScriptIndexInputs : whitelisted_t {
	std::map<hash160_t, hash160_t> txOutMap;

	bool initialize(const char* arg) {
		if (whitelisted_t::initialize(arg)) return true;
		if (strncmp(arg, "-fm", 3) == 0) {
			const auto fileName = std::string(arg + 3);
			const auto file = fopen(fileName.c_str(), "r");
			assert(file != nullptr);

			// read mapped txOuts from file until EOF
			while (true) {
				uint8_t rbuf[40];
				const auto read = fread(rbuf, sizeof(rbuf), 1, file);

				// EOF?
				if (read == 0) break;

				hash160_t key, value;
				memcpy(&key[0], rbuf, 20);
				memcpy(&value[0], rbuf, 20);

				this->txOutMap.emplace(key, value);
			}

			fclose(file);
			std::cerr << "Read " << this->txOutMap.size() << " txOuts" << std::endl;

			return true;
		}

		return false;
	}

	void operator()(const Block& block) const {
		assert(!this->txOutMap.empty());

		hash256_t hash;
		hash256(&hash[0], block.header);
		if (this->skipHash(hash)) return;

		uint8_t sbuf[84] = {};
		memcpy(sbuf, &hash[0], 32);

		auto transactions = block.transactions();
		while (!transactions.empty()) {
			const auto& transaction = transactions.front();

			hash256(sbuf + 32, transaction.data);

			for (const auto& input : transaction.inputs) {
				// Coinbase input? skip
				if (
					input.vout == 0xffffffff &&
					memcmp(input.hash.begin, COINBASE, sizeof(COINBASE)) == 0
				) {
					continue;
				}

				hash160_t hash;
				sha1(&hash[0], input.data.take(36));

				const auto txOutMapIter = this->txOutMap.find(hash);
				assert(txOutMapIter != this->txOutMap.end());

				memcpy(sbuf + 64, &(txOutMapIter->second)[0], 20);
				fwrite(sbuf, sizeof(sbuf), 1, stdout);
			}

			transactions.popFront();
		}
	}
};
