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
struct TypedFixedSlice {
	T* begin;
	T* end;

	TypedFixedSlice () : begin(nullptr), end(nullptr) {}
	TypedFixedSlice (T* begin, T* end) : begin(begin), end(end) {
		assert(this->begin);
		assert(this->end);
	}

	auto drop (size_t n) const;
	auto take (size_t n) const;

	size_t length () const {
		return static_cast<size_t>(this->end - this->begin) / sizeof(T);
	}

	template <typename Y = T>
	auto peek (const size_t offset = 0) const {
		assert(offset + sizeof(Y) <= this->length());
		return *(reinterpret_cast<Y*>(this->begin + offset));
	}

	template <typename Y, bool swap = false>
	void put (const Y value, size_t offset = 0) {
		assert(offset + sizeof(Y) <= this->length());
		auto ptr = reinterpret_cast<Y*>(this->begin + offset);
		*ptr = value;

		if (swap) swapEndian(ptr);
	}

	auto& operator[] (const size_t i) {
		assert(i < this->length());
		return this->begin[i];
	}

	const auto operator[] (const size_t i) const {
		assert(i < this->length());
		return this->begin[i];
	}
};

template <typename T>
struct TypedSlice : public TypedFixedSlice<T> {
	TypedSlice () : TypedFixedSlice<T>() {}
	TypedSlice (T* begin, T* end) : TypedFixedSlice<T>(begin, end) {}

	void popFrontN (size_t n) {
		assert(n <= this->length());
		this->begin += n;
	}

	void popFront () {
		this->popFrontN(1);
	}

	template <typename Y>
	auto read () {
		const auto value = this->template peek<Y>();
		this->popFrontN(sizeof(T) / sizeof(Y));
		return value;
	}

	template <typename Y, bool swap = false>
	void write (const Y value) {
		this->template put<Y, swap>(value);
		this->popFrontN(sizeof(T) / sizeof(Y));
	}

	template <typename Y>
	void writeN (const Y* data, size_t n) {
		assert(n >= this->length());
		memcpy(this->begin, data, n);
		this->popFrontN(n);
	}
};

template <typename T>
auto TypedFixedSlice<T>::drop (const size_t n) const {
	assert(n <= this->length());
	return TypedSlice<T>(this->begin + n, this->end);
}

template <typename T>
auto TypedFixedSlice<T>::take (const size_t n) const {
	assert(n <= this->length());
	return TypedSlice<T>(this->begin, this->begin + n);
}

template <typename T, size_t N>
struct TypedStackSlice : TypedFixedSlice<T> {
	T data[N];

	TypedStackSlice () : TypedFixedSlice<T>(data, data + N) {}
// 	TypedStackSlice (const TypedStackSlice& copy) {
// 		memcpy(this->begin, copy.begin, N);
// 	}
};

template <typename T>
struct TypedHeapSlice : TypedFixedSlice<T> {
	TypedHeapSlice (const size_t n) : TypedFixedSlice<T>() {
		this->begin = new T[n];
		this->end = this->begin + n;
	}

	~TypedHeapSlice () {
		delete[] this->begin;
	}
};

typedef TypedHeapSlice<uint8_t> HeapSlice;

template <size_t N>
using StackSlice = TypedStackSlice<uint8_t, N>;

typedef TypedSlice<uint8_t> Slice;
