#pragma once

#include <algorithm>
#include <cassert>

#include "endian.h"
#if __BYTE_ORDER != __LITTLE_ENDIAN
#error BIG_ENDIAN systems unsupported
#endif

namespace {
	template <typename T>
	void swapEndian (T* p) {
		uint8_t* q = reinterpret_cast<uint8_t*>(p);

		std::reverse(q, q + sizeof(T));
	}
}

template <typename T>
struct TypedFixedSlice {
protected:
	T* _begin;
	T* _end;

public:
	TypedFixedSlice () : _begin(nullptr), _end(nullptr) {}
	TypedFixedSlice (T* begin, T* end) : _begin(begin), _end(end) {
		assert(this->_begin);
		assert(this->_end);
	}

	auto begin () const { return this->_begin; }
	auto end () const { return this->_end; }

	auto dup () const;
	auto drop (size_t n) const;
	auto take (size_t n) const;

	size_t length () const {
		return static_cast<size_t>(this->_end - this->_begin);
	}

	template <typename Y = T>
	auto peek (const size_t offset = 0) const {
		static_assert(sizeof(Y) % sizeof(T) == 0);

		assert(offset + (sizeof(Y) / sizeof(T)) <= this->length());
		return *(reinterpret_cast<Y*>(this->_begin + offset));
	}

	template <typename Y, bool bigEndian = false>
	void put (const Y value, size_t offset = 0) {
		static_assert(sizeof(Y) % sizeof(T) == 0);

		assert(offset + (sizeof(Y) / sizeof(T)) <= this->length());
		auto ptr = reinterpret_cast<Y*>(this->_begin + offset);
		*ptr = value;

		if (bigEndian) swapEndian(ptr);
	}

	auto& operator[] (const size_t i) {
		assert(i < this->length());
		return this->_begin[i];
	}

	auto operator[] (const size_t i) const {
		assert(i < this->length());
		return this->_begin[i];
	}

	auto operator< (const TypedFixedSlice<T>& rhs) const {
		return std::lexicographical_compare(this->_begin, this->_end, rhs._begin, rhs._end);
	}
};

template <typename T>
struct TypedSlice : public TypedFixedSlice<T> {
	TypedSlice () : TypedFixedSlice<T>() {}
	TypedSlice (T* begin, T* end) : TypedFixedSlice<T>(begin, end) {}

	void popFrontN (size_t n) {
		assert(n <= this->length());
		this->_begin += n;
	}

	void popFront () {
		this->popFrontN(1);
	}

	template <typename Y>
	auto read () {
		static_assert(sizeof(Y) % sizeof(T) == 0);

		const auto value = this->template peek<Y>();
		this->popFrontN(sizeof(Y) / sizeof(T));
		return value;
	}

	auto readN (const size_t n) {
		const auto slice = this->take(n);
		this->popFrontN(n);
		return slice;
	}

	template <typename Y, bool bigEndian = false>
	void write (const Y value) {
		static_assert(sizeof(Y) % sizeof(T) == 0);

		this->template put<Y, bigEndian>(value);
		this->popFrontN(sizeof(Y) / sizeof(T));
	}

	// TODO: use std::copy / reverse iterators etc
	void writeN (const T* data, size_t n) {
		assert(n <= this->length());
		memcpy(this->_begin, data, n);
		this->popFrontN(n);
	}

	void writeNReverse (const T* data, size_t n) {
		assert(n <= this->length());

		for (size_t i = 0; i < n; ++i) {
			this->_begin[i] = data[n - 1 - i];
		}

		this->popFrontN(n);
	}
};

template <typename T>
auto TypedFixedSlice<T>::dup () const {
	return TypedSlice<T>(this->_begin, this->_end);
}

template <typename T>
auto TypedFixedSlice<T>::drop (const size_t n) const {
	assert(n <= this->length());
	return TypedSlice<T>(this->_begin + n, this->_end);
}

template <typename T>
auto TypedFixedSlice<T>::take (const size_t n) const {
	assert(n <= this->length());
	return TypedSlice<T>(this->_begin, this->_begin + n);
}

template <typename T, size_t N>
struct TypedStackSlice : TypedFixedSlice<T> {
private:
	T data[N];

public:
	TypedStackSlice () : TypedFixedSlice<T>(data, data + N) {}
// 	TypedStackSlice (const TypedStackSlice& copy) {
// 		memcpy(this->_begin, copy._begin, N);
// 	}
};

template <typename T>
struct TypedHeapSlice : TypedFixedSlice<T> {
	TypedHeapSlice (const size_t n) : TypedFixedSlice<T>() {
		this->_begin = new T[n];
		this->_end = this->_begin + n;
	}

	~TypedHeapSlice () {
		delete[] this->_begin;
	}
};

typedef TypedHeapSlice<uint8_t> HeapSlice;

template <size_t N>
using StackSlice = TypedStackSlice<uint8_t, N>;

typedef TypedSlice<uint8_t> Slice;
