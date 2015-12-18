#pragma once

#include "libconsensus/sha1.h"
#include "libconsensus/sha256.h"

void sha1 (uint8_t* dest, const uint8_t* src, size_t n) {
	CSHA1 hash;
	hash.Write(src, n);
	hash.Finalize(dest);
}

void hash256 (uint8_t* dest, const uint8_t* src, size_t n) {
	CSHA256 hash;
	hash.Write(src, n);
	hash.Finalize(dest);
	hash.Reset();
	hash.Write(dest, 32);
	hash.Finalize(dest);
}

// Slice wrappers
#include "slice.hpp"

void sha1 (uint8_t* dest, const Slice src) {
	sha1(dest, src.begin, src.length());
}

void hash256 (uint8_t* dest, const Slice src) {
	hash256(dest, src.begin, src.length());
}
