#pragma once
#include "transform_base.hpp"

// BLOCK_HEADER > stdout
struct dumpHeaders : transform_t {
	void operator() (const Block& block) {
		if (this->shouldSkip(block)) return;

		fwrite(block.header.begin, 80, 1, stdout);
	}
};
