#include <algorithm>
#include <cstdio>
#include <cstring>

#include "pg.hpp"
#include "slice.hpp"

int main () {
	fwrite(PG_BINARY_HEADER, sizeof(PG_BINARY_HEADER), 1, stdout);
	uint32_t height = 0;

	while (true) {
		uint8_t buffer[32];
		const auto read = fread(buffer, sizeof(buffer), 1, stdin);

		// EOF?
		if (read == 0) break;

		uint8_t pbuffer[46];
		auto pslice = Slice(pbuffer, pbuffer + sizeof(pbuffer));

		std::reverse(&buffer[0], &buffer[32]); // BLOCK_HASH -> BLOCK_ID

		// postgres COPY tuple
		pslice.write<int16_t, true>(2);

		pslice.write<int32_t, true>(32);
		memcpy(pslice.begin, buffer, 32);
		pslice.popFrontN(32);

		pslice.write<int32_t, true>(4);
		pslice.write<int32_t, true>(height);
		fwrite(pbuffer, sizeof(pbuffer), 1, stdout);

		++height;
	}

	fwrite(PG_BINARY_TAIL, sizeof(PG_BINARY_TAIL), 1, stdout);

	return 0;
}
