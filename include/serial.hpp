// TODO: probaby unrelated to ranges, but
#pragma once

#include <algorithm>
#include "ranger.hpp"

#include "endian.h"
#if __BYTE_ORDER != __LITTLE_ENDIAN
#error "big endian architecture not supported"
#endif

namespace serial {
	template <typename E, bool BE = false, typename R>
	auto peek (const R& r) {
		using T = typename R::value_type;

		static_assert(std::is_same<T, uint8_t>::value);
		static_assert(sizeof(E) % sizeof(T) == 0);

		constexpr auto count = sizeof(E) / sizeof(T);
		assert(count <= r.size());

		E value;
		auto copy = range(r);
		auto ptr = reinterpret_cast<T*>(&value);

		if (BE) {
			for (size_t i = 0; i < count; ++i, copy.popFront()) ptr[count - 1 - i] = copy.front();
		} else {
			for (size_t i = 0; i < count; ++i, copy.popFront()) ptr[i] = copy.front();
		}

		return value;
	}

	template <typename E, bool BE = false, typename R>
	void place (R& r, const E e) {
		using T = typename R::value_type;

		static_assert(std::is_same<T, uint8_t>::value);
		static_assert(sizeof(E) % sizeof(T) == 0);

		constexpr auto count = sizeof(E) / sizeof(T);
		assert(count <= r.size());

		auto copy = range(r);
		auto ptr = reinterpret_cast<const T*>(&e);

		if (BE) {
			for (size_t i = 0; i < count; ++i, copy.popFront()) copy.front() = ptr[count - 1 - i];
		} else {
			for (size_t i = 0; i < count; ++i, copy.popFront()) copy.front() = ptr[i];
		}
	}

	template <typename E, bool BE = false, typename R>
	auto read (R& r) {
		using T = typename R::value_type;

		const auto e = peek<E, BE, R>(r);
		r.popFrontN(sizeof(E) / sizeof(T));
		return e;
	}

	template <typename E, bool BE = false, typename R>
	void put (R& r, const E e) {
		using T = typename R::value_type;

		place<E, BE, R>(r, e);
		r.popFrontN(sizeof(E) / sizeof(T));
	}

	// rvalue references wrappers
	template <typename E, bool BE = false, typename R> void place (R&& r, const E e) { place<E, BE, R>(r, e); }
	template <typename E, bool BE = false, typename R> auto read (R&& r) { return read<E, BE, R>(r); }
	template <typename E, bool BE = false, typename R> void put (R&& r, const E e) { put<E, BE, R>(r, e); }
}
