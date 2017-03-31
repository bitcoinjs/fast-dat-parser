#pragma once

#include <cassert>
#include <iterator>
#include <type_traits>

namespace __ranger {
	template <typename R>
	auto drop (R r, const size_t n) {
		r.popFrontN(n);
		return std::move(r);
	}

	// TODO: specialization for ranges without .size()
	template <typename R>
	auto take (R r, const size_t n) {
		r.popBackN(r.size() - n);
		return std::move(r);
	}

	template <typename R, typename E>
	void put (R& r, E e) {
		while (!e.empty()) {
			r.front() = e.front();
			r.popFront();
			e.popFront();
		}
	}

	// TODO
// 	template <typename R, typename E = typename R::value_type>
// 	void put (R& r, typename R::value_type e) {
// 		r.front() = e;
// 		r.popFront();
// 	}

	template <typename I>
	struct Range {
	private:
		I _begin;
		I _end;

	public:
		using value_type = typename std::remove_reference<decltype(*I())>::type;

		Range (I begin, I end) : _begin(begin), _end(end) {}

		auto begin () const { return this->_begin; }
		auto drop (size_t n) const { return __ranger::drop(*this, n); }
		auto empty () const { return this->_begin == this->_end; }
		auto end () const { return this->_end; }
		auto size () const {
			const auto diff = std::distance(this->_begin, this->_end);
			assert(diff >= 0);
			return static_cast<size_t>(diff);
		}
		auto take (size_t n) const { return __ranger::take(*this, n); }
		auto& back () { return *(this->_end - 1); }
		auto& front () { return *this->_begin; }
		void popBack () { this->popBackN(1); }
		void popBackN (size_t n) {
			assert(n <= this->size());
			std::advance(this->_end, -n);
		}
		void popFront () { this->popFrontN(1); }
		void popFrontN (size_t n) {
			assert(n <= this->size());
			std::advance(this->_begin, n);
		}

		template <typename E>
		void put (E e) { return __ranger::put(*this, e); }

		auto& operator[] (const size_t i) {
			assert(i < this->size());
			return *std::next(this->_begin, i);
		}

		auto operator[] (const size_t i) const {
			assert(i < this->size());
			return *std::next(this->_begin, i);
		}

		template <typename E>
		auto operator< (const E& rhs) const {
			return std::lexicographical_compare(this->begin(), this->end(), rhs.begin(), rhs.end());
		}
	};
}

template <typename R>
auto range (R& r) {
	using iterator = decltype(r.begin());

	return __ranger::Range<iterator>(r.begin(), r.end());
}

template <typename R>
auto ptr_range (R& r) {
	using pointer = decltype(r.data());

	return __ranger::Range<pointer>(r.data(), r.data() + r.size());
}

template <typename R>
auto retro (R& r) {
	using reverse_iterator = std::reverse_iterator<decltype(r.begin())>;

	return __ranger::Range<reverse_iterator>(reverse_iterator(r.end()), reverse_iterator(r.begin()));
}

// rvalue references wrappers
template <typename R> auto range (R&& r) { return range<R>(r); }
template <typename R> auto ptr_range (R&& r) { return ptr_range<R>(r); }
template <typename R> auto retro (R&& r) { return retro<R>(r); }
