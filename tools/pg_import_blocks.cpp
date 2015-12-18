#include <algorithm>
#include <cstdio>

#include <iostream>
#include <iomanip>

#include "pg.hpp"
#include "slice.hpp"

int main () {
	uint32_t height = 0;

	std::cout << "COPY blocks (id, height) FROM STDIN BINARY" << std::endl;
	fwrite(PG_BINARY_HEADER, sizeof(PG_BINARY_HEADER), 1, stdout);

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
		pslice.write(buffer, 32);

		pslice.write<int32_t, true>(4);
		pslice.write<int32_t, true>(height);

		++height;
	}

	fwrite(PG_BINARY_TAIL, sizeof(PG_BINARY_TAIL), 1, stdout);

	return 0;
}
