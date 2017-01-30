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

template <typename T>
struct TypedSlice {
	T* begin;
	T* end;

	TypedSlice() : begin(nullptr), end(nullptr) {}
	TypedSlice(T* begin, T* end) : begin(begin), end(end) {
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
		return TypedSlice<T>(this->begin + n, this->end);
	}

	auto take (size_t n) const {
		assert(n <= this->length());
		return TypedSlice<T>(this->begin, this->begin + n);
	}

	size_t length () const {
		return static_cast<size_t>(this->end - this->begin) / sizeof(T);
	}

	template <typename Y = T>
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

	template <typename Y = T>
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

template <size_t N, typename T>
struct TypedStackSlice {
	T data[N];

	auto drop (size_t n) const {
		assert(n <= N);
		return TypedSlice<T>(this->data + n, this->data + N);
	}

	auto take (size_t n) const {
		assert(n <= N);
		return TypedSlice<T>(this->data, this->data + n);
	}

	auto length () const {
		return N;
	}

	template <typename Y = T>
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

template <typename T>
struct TypedHeapSlice {
	T* begin;
	T* end;

	TypedHeapSlice (size_t n) : begin(new T[n]) {
		this->end = this->begin + n;
	}

	~TypedHeapSlice () {
		delete[] this->begin;
	}

	auto drop (size_t n) const {
		assert(n <= this->length());
		return TypedSlice<T>(this->begin + n, this->end);
	}

	auto take (size_t n) const {
		assert(n <= this->length());
		return TypedSlice<T>(this->begin, this->begin + n);
	}

	size_t length () const {
		return static_cast<size_t>(this->end - this->begin);
	}

	template <typename Y = T>
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

typedef TypedSlice<uint8_t> Slice;
typedef TypedHeapSlice<uint8_t> HeapSlice;

template <size_t N>
using StackSlice = TypedStackSlice<N, uint8_t>;
