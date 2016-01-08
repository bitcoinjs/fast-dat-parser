#pragma once

#include <algorithm>
#include <cassert>

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
	auto peek (size_t offset = 0) const {
		assert(offset + sizeof(Y) <= this->length());
		return *(reinterpret_cast<Y*>(this->begin + offset));
	}

	template <typename Y, bool swap = false>
	void put (Y value, size_t offset = 0) {
		assert(offset + sizeof(Y) <= this->length());
		auto ptr = reinterpret_cast<Y*>(this->begin + offset);
		*ptr = value;

		if (swap) swapEndian(ptr);
	}

	template <typename Y = uint8_t>
	auto read () {
		const Y value = this->peek<Y>();
		this->popFrontN(sizeof(Y));
		return value;
	}

	template <typename Y, bool swap = false>
	void write (Y value) {
		this->put<Y, swap>(value);
		this->popFrontN(sizeof(Y));
	}

	auto& operator[](const size_t i) {
		assert(i < this->length());
		return this->begin[i];
	}

	const auto operator[](const size_t i) const {
		assert(i < this->length());
		return this->begin[i];
	}
};

template <size_t N>
struct ArraySlice {
	uint8_t data[N];

	auto drop (size_t n) const {
		assert(n <= N);
		return Slice(this->data + n, this->data + N);
	}

	auto take (size_t n) const {
		assert(n <= N);
		return Slice(this->data, this->data + n);
	}

	auto length () const {
		return N;
	}

	template <typename Y = uint8_t>
	auto peek (size_t offset = 0) const {
		assert(offset + sizeof(Y) <= N);
		return *(reinterpret_cast<Y*>(this->data + offset));
	}

	template <typename Y, bool swap = false>
	void put (Y value, size_t offset = 0) {
		assert(offset + sizeof(Y) <= N);
		auto ptr = reinterpret_cast<Y*>(this->data + offset);
		*ptr = value;

		if (swap) swapEndian(ptr);
	}

	auto& operator[](const size_t i) {
		assert(i < N);
		return this->data[i];
	}

	const auto operator[](const size_t i) const {
		assert(i < N);
		return this->data[i];
	}
};

struct FixedSlice {
	uint8_t* begin;
	uint8_t* end;

	FixedSlice (size_t n) : begin(new uint8_t[n]) {
		this->end = this->begin + n;
	}

	~FixedSlice () {
		delete[] this->begin;
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
	auto peek (size_t offset = 0) const {
		assert(offset + sizeof(Y) <= this->length());
		return *(reinterpret_cast<Y*>(this->begin + offset));
	}

	template <typename Y, bool swap = false>
	void put (Y value, size_t offset = 0) {
		assert(offset + sizeof(Y) <= this->length());
		auto ptr = reinterpret_cast<Y*>(this->begin + offset);
		*ptr = value;

		if (swap) swapEndian(ptr);
	}

	auto& operator[](const size_t i) {
		assert(i < this->length());
		return this->begin[i];
	}

	const auto operator[](const size_t i) const {
		assert(i < this->length());
		return this->begin[i];
	}
};
