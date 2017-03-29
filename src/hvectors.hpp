#pragma once

#include <algorithm>
#include <vector>

template <typename K, typename V>
struct HMap : std::vector<std::pair<K, V>> {
	auto find (const K& key) const {
		const auto iter = std::lower_bound(
			this->begin(), this->end(), key,
			[](const auto& pair, const K& key) {
				return pair.first < key;
			}
		);

		if (iter == this->end()) return this->end();
		if (iter->first != key) return this->end();
		return iter;
	}

	void insort (const K& key, const V& value) {
		const auto iter = std::lower_bound(
			this->begin(), this->end(), key,
			[](const auto& pair, const K& key) {
				return pair.first < key;
			}
		);

		this->emplace(iter, std::make_pair(key, value));
	}

	auto ready () const {
		return std::is_sorted(
			this->begin(),
			this->end(),
			[](const auto& a, const auto& b) {
				return a.first < b.first;
			}
		);
	}

	void sort () {
		std::sort(
			this->begin(),
			this->end(),
			[](const auto& a, const auto& b) {
				return a.first < b.first;
			}
		);
	}
};
