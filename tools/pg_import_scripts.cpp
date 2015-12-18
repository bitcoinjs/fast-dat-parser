#include <algorithm>
#include <cstdio>

#include <map>
#include <iostream>
#include <iomanip>

#include "pg.hpp"
#include "slice.hpp"

typedef std::array<uint8_t, 32> hash_t;

auto importChainHeightMap (const std::string& fileName) {
	std::map<hash_t, uint32_t> map;

	auto file = fopen(fileName.c_str(), "r");
	if (file == nullptr) return map;

	uint32_t height = 0;
	do {
		hash_t hash;
		const auto read = fread(&hash[0], 32, 1, file);

		// EOF?
		if (read == 0) break;

		map[hash] = height;
		++height;
	} while (true);

	return map;
}

int main (int argc, char** argv) {
	if (argc < 2) return 1;

	const auto headersFileName = std::string(argv[1]);
	const auto chainHeightMap = importChainHeightMap(headersFileName);
	if (chainHeightMap.empty()) return 1;

	fputs("COPY scripts (id, txid, height) FROM STDIN BINARY\r\n", stdout);
	fwrite(PG_BINARY_HEADER, sizeof(PG_BINARY_HEADER), 1, stdout);

	while (true) {
		uint8_t buffer[84];
		const auto read = fread(buffer, sizeof(buffer), 1, stdin);

		// EOF?
		if (read == 0) break;

		hash_t block_hash;
		memcpy(&block_hash[0], &buffer[0], 32);

		const auto heightPair = chainHeightMap.find(block_hash);
		if (heightPair == chainHeightMap.end()) continue;

		std::reverse(&buffer[32], &buffer[64]); // TX_HASH -> TX_ID

		uint8_t pbuffer[70];
		auto pslice = Slice(pbuffer, pbuffer + sizeof(pbuffer));

		// postgres COPY tuple
		pslice.write<int16_t, true>(3);

		pslice.write<int32_t, true>(20);
		pslice.write(buffer + 64, 20);

		pslice.write<int32_t, true>(32);
		pslice.write(buffer + 32, 32);

		pslice.write<int32_t, true>(4);
		pslice.write<int32_t>(heightPair->second);

		fwrite(pbuffer, sizeof(pbuffer), 1, stdout);
	}

	fwrite(PG_BINARY_TAIL, sizeof(PG_BINARY_TAIL), 1, stdout);

	return 0;
}
