// XXX: fwrite can be used without sizeof(sbuf) < PIPE_BUF (4096 bytes)
#pragma once

#include "block.hpp"
#include "hash.hpp"
#include "hvectors.hpp"

struct transform_t_base {
	virtual ~transform_t_base () {}

	virtual bool initialize (const char*) { return false; }
	virtual void operator() (const Block&) = 0;
};

struct transform_t : transform_t_base {
	HMap<hash256_t, uint32_t> whitelist;

	bool initialize (const char* arg) {
		if (strncmp(arg, "-w", 2) == 0) {
			const auto fileName = std::string(arg + 2);
			const auto file = fopen(fileName.c_str(), "r");
			assert(file != nullptr);

			fseek(file, 0, SEEK_END);
			const auto fileSize = ftell(file);
			fseek(file, 0, SEEK_SET);

			const auto elementSize = sizeof(this->whitelist.front());
			assert(elementSize == 36);
			assert((fileSize % elementSize) == 0);

			this->whitelist.resize(fileSize / elementSize);
			const auto read = fread(&this->whitelist[0], fileSize, 1, file);
			assert(read == 1);

			fclose(file);
			assert(not this->whitelist.empty());
			assert(std::is_sorted(this->whitelist.begin(), this->whitelist.end()));

			std::cerr << "Whitelisted " << this->whitelist.size() << " hashes" << std::endl;
			return true;
		}

		return false;
	}

	bool shouldSkip (const Block& block, hash256_t* _hash = nullptr, uint32_t* _height = nullptr) const {
		if (this->whitelist.empty()) return false;

		hash256_t hash;
		hash256(hash.begin(), block.header);
		auto iter = this->whitelist.find(hash);
		if (iter == this->whitelist.end()) return false;

		if (_hash != nullptr) *_hash = iter->first;
		if (_height != nullptr) *_height = iter->second;
		return true;
	}
};
