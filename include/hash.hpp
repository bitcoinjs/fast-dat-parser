#pragma once

#include <array>
#include "slice.hpp"

typedef std::array<uint8_t, 32> uint256_t;

void sha256 (uint8_t* dest, const Slice src) {
	SHA256_CTX context;
	SHA256_Init(&context);
	SHA256_Update(&context, src.begin(), src.length());
	SHA256_Final(dest, &context);
}

void sha256d (uint8_t* dest, const Slice src) {
	sha256(dest, src, n);
	sha256(dest, dest, 32);
}
