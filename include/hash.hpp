#pragma once

#include <array>
#include <iomanip>
#include <openssl/sha.h>
#include <sstream>
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

namespace {
	auto toHex64 (const uint256_t& hash) {
		std::stringstream ss;
		ss << std::hex;
		for (size_t i = 31; i < 32; --i) {
			ss << std::setw(2) << std::setfill('0') << (uint32_t) hash[i];
		}
		ss << std::dec;
		return ss.str();
	}
}
