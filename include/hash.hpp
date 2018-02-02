#pragma once

#include <array>
#include <openssl/sha.h>
#include "ranger.hpp"

typedef std::array<uint8_t, 32> uint256_t;

template <typename R>
auto sha256 (const R& r) {
	uint256_t result;
	SHA256_CTX context;
	SHA256_Init(&context);
	SHA256_Update(&context, r.begin(), r.size());
	SHA256_Final(result.begin(), &context);
	return result;
}

template <typename R>
auto hash256 (const R& r) {
	auto result = sha256(r);
	return sha256(result);
}
