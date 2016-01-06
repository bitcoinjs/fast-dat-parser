#pragma once

#include "libconsensus/ripemd160.h"
#include "libconsensus/sha1.h"
#include "libconsensus/sha256.h"

void hash160 (uint8_t* dest, const uint8_t* src, size_t n) {
	CSHA256 hash;
	hash.Write(src, n);
	hash.Finalize(dest);
	CRIPEMD160 hashR;
	hashR.Write(dest, 32);
	hashR.Finalize(dest);
}

void hash256 (uint8_t* dest, const uint8_t* src, size_t n) {
	CSHA256 hash;
	hash.Write(src, n);
	hash.Finalize(dest);
	hash.Reset();
	hash.Write(dest, 32);
	hash.Finalize(dest);
}

void ripemd160 (uint8_t* dest, const uint8_t* src, size_t n) {
	CRIPEMD160 hash;
	hash.Write(src, n);
	hash.Finalize(dest);
}

void sha1 (uint8_t* dest, const uint8_t* src, size_t n) {
	CSHA1 hash;
	hash.Write(src, n);
	hash.Finalize(dest);
}

void sha256 (uint8_t* dest, const uint8_t* src, size_t n) {
	CSHA256 hash;
	hash.Write(src, n);
	hash.Finalize(dest);
}

// Slice wrappers
#include "slice.hpp"

void hash160 (uint8_t* dest, const Slice src) { hash160(dest, src.begin, src.length()); }
void hash256 (uint8_t* dest, const Slice src) { hash256(dest, src.begin, src.length()); }
void ripemd160 (uint8_t* dest, const Slice src) { ripemd160(dest, src.begin, src.length()); }
void sha1 (uint8_t* dest, const Slice src) { sha1(dest, src.begin, src.length()); }
void sha256 (uint8_t* dest, const Slice src) { sha256(dest, src.begin, src.length()); }
