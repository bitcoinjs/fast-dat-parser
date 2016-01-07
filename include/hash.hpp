#pragma once

#include <array>
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

// Convenience wrappers
#include "slice.hpp"

typedef std::array<uint8_t, 20> hash160_t;
typedef std::array<uint8_t, 32> hash256_t;

auto hash160(const uint8_t* src, size_t n) {
	hash160_t tmp;
	hash160(&tmp[0], src, n);
	return tmp;
}

auto hash256(const uint8_t* src, size_t n) {
	hash256_t tmp;
	hash256(&tmp[0], src, n);
	return tmp;
}

auto ripemd160(const uint8_t* src, size_t n) {
	hash160_t tmp;
	ripemd160(&tmp[0], src, n);
	return tmp;
}

auto sha1 (const uint8_t* src, size_t n) {
	hash160_t tmp;
	sha1(&tmp[0], src, n);
	return tmp;
}

auto sha256 (const uint8_t* src, size_t n) {
	hash256_t tmp;
	sha256(&tmp[0], src, n);
	return tmp;
}

auto hash160 (const Slice src) { return hash160(src.begin, src.length()); }
auto hash256 (const Slice src) { return hash256(src.begin, src.length()); }
auto ripemd160 (const Slice src) { return ripemd160(src.begin, src.length()); }
auto sha1 (const Slice src) { return sha1(src.begin, src.length()); }
auto sha256 (const Slice src) { return sha256(src.begin, src.length()); }

void hash160 (uint8_t* dest, const Slice src) { hash160(dest, src.begin, src.length()); }
void hash256 (uint8_t* dest, const Slice src) { hash256(dest, src.begin, src.length()); }
void ripemd160 (uint8_t* dest, const Slice src) { ripemd160(dest, src.begin, src.length()); }
void sha1 (uint8_t* dest, const Slice src) { sha1(dest, src.begin, src.length()); }
void sha256 (uint8_t* dest, const Slice src) { sha256(dest, src.begin, src.length()); }
