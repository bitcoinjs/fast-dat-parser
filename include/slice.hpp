#pragma once

#include <algorithm>
#include <cassert>
#include <cstring>

namespace {
	template <typename T>
	void swapEndian (T* p) {
		uint8_t* q = reinterpret_cast<uint8_t*>(p);

		std::reverse(q, q + sizeof(T));
	}
}

struct Slice {
	uint8_t* begin;
	uint8_t* end;

	Slice() : begin(nullptr), end(nullptr) {}
	Slice(uint8_t* begin, uint8_t* end) : begin(begin), end(end) {
		assert(this->begin);
		assert(this->end);
	}

	void popFrontN (size_t n) {
		assert(n <= this->length());
		this->begin += n;
	}

	void popFront () {
		this->popFrontN(1);
	}

	auto drop (size_t n) const {
		assert(n <= this->length());
		return Slice(this->begin + n, this->end);
	}

	auto take (size_t n) const {
		assert(n <= this->length());
		return Slice(this->begin, this->begin + n);
	}

	size_t length () const {
		return static_cast<size_t>(this->end - this->begin);
	}

	template <typename Y = uint8_t>
	Y peek (size_t offset = 0) const {
		return *(reinterpret_cast<Y*>(this->begin + offset));
	}

	template <typename Y, bool swap = false>
	void put (Y value, size_t offset = 0) {
		assert(offset + sizeof(Y) <= this->length());
		auto ptr = reinterpret_cast<Y*>(this->begin + offset);
		*ptr = value;

		if (swap) swapEndian(ptr);
	}

	void put (uint8_t* value, size_t n, size_t offset = 0) {
		assert(offset + n <= this->length());
		memcpy(this->begin + offset, value, n);
	}

	void put (Slice value, size_t offset = 0) {
		this->put(value.begin, value.length(), offset);
	}

	template <typename Y = uint8_t>
	Y read () {
		const Y value = this->peek<Y>();
		this->popFrontN(sizeof(Y));
		return value;
	}

	template <typename Y, bool swap = false>
	void write (Y value) {
		this->put<Y, swap>(value);
		this->popFrontN(sizeof(Y));
	}

	void write (Slice value) {
		this->put(value);
		this->popFrontN(value.length());
	}

	void write (uint8_t* value, size_t n) {
		this->put(value, n);
		this->popFrontN(n);
	}

	uint8_t& operator[](const size_t i) {
		assert(i <= this->length());
		return this->begin[i];
	}

	const uint8_t& operator[](const size_t i) const {
		assert(i <= this->length());
		return this->begin[i];
	}
};
