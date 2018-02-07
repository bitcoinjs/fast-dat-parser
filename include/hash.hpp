#pragma once

#include <array>
#include <iomanip>
#include <openssl/sha.h>
#include <sstream>
#include "hexxer.hpp"
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
	template <typename R>
	void putHex (R& output, const R& data) {
		auto save = range(data);

		while (not save.empty()) {
			hex_encode(reinterpret_cast<char*>(output.begin()), save.begin(), 1);
			output.popFrontN(2);
			save.popFrontN(1);
		}
	}

	template <typename R>
	auto toHex (const R& data) {
		auto save = range(data);
		std::array<char, 2> buffer;
		std::string str;
		str.reserve(save.size() * 2);

		while (not save.empty()) {
			const auto byte = save.front();
			hex_encode(reinterpret_cast<char*>(buffer.begin()), &byte, 1);
			save.popFront();
			str.push_back(buffer[0]);
			str.push_back(buffer[1]);
		}

		return str;
	}

	auto toHexBE (const uint256_t& hash) {
		return toHex(retro(hash));
	}
}
