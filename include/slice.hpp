#pragma once

#include <cassert>
#include <memory>

template <typename T>
struct Slice {
	uint8_t* begin;
	uint8_t* end;

	Slice() : begin(nullptr), end(nullptr) {}
	Slice(uint8_t* begin, uint8_t* end) : begin(begin), end(end) {
		assert(begin);
		assert(end);
	}

	void popFrontN (size_t n) {
		assert(this->length() >= n);
		begin += n;
	}

	void popFront () {
		this->popFrontN(1);
	}

	T front () const {
		assert(this->length() > 0);
		return this->peek<T>();
	}

	auto drop (size_t n) const {
		assert(this->length() >= n);
		return Slice<T>(this->begin + n, this->end);
	}

	auto take (size_t n) const {
		assert(this->length() >= n);
		return Slice<T>(this->begin, this->begin + n);
	}

	auto length () const {
		return static_cast<size_t>(this->end - this->begin);
	}

	auto size() const {
		return this->length();
	}

	template <typename Y>
	Y peek () const {
		return *(reinterpret_cast<Y*>(this->begin));
	}

	template <typename Y>
	Y read () {
		const Y value = this->peek<Y>();
		this->popFrontN(sizeof(Y));
		return value;
	}

	T& operator[](const size_t i) {
		assert(this->length() >= i);
		return this->begin[i];
	}

	const T& operator[](const size_t i) const {
		assert(this->length() >= i);
		return this->begin[i];
	}
};
