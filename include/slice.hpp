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

	auto drop (size_t n) const;
	auto take (size_t n) const;

	size_t length () const {
		return static_cast<size_t>(this->_end - this->_begin) / sizeof(T);
	}

	template <typename Y = T>
	auto peek (const size_t offset = 0) const {
		assert(offset + sizeof(Y) <= this->length());
		return *(reinterpret_cast<Y*>(this->_begin + offset));
	}

	template <typename Y, bool swap = false>
	void put (const Y value, size_t offset = 0) {
		assert(offset + sizeof(Y) <= this->length());
		auto ptr = reinterpret_cast<Y*>(this->_begin + offset);
		*ptr = value;

		if (swap) swapEndian(ptr);
	}

	auto& operator[] (const size_t i) {
		assert(i < this->length());
		return this->_begin[i];
	}

	const auto operator[] (const size_t i) const {
		assert(i < this->length());
		return this->_begin[i];
	}

	const auto operator< (const TypedFixedSlice<T>& rhs) const {
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
		assert(n <= this->length());
		memcpy(this->_begin, data, n);
		this->popFrontN(n);
	}
};

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
