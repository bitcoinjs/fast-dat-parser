#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

#include "hash.hpp"
#include "slice.hpp"

namespace {
	auto readSlice (Slice& data, uint64_t n) {
		auto slice = data.take(n);
		data.popFrontN(n);
		return slice;
	}

	auto readVI (Slice& data) {
		const auto i = data.read<uint8_t>();

		if (i < 253) return static_cast<uint64_t>(i);
		if (i < 254) return static_cast<uint64_t>(data.read<uint16_t>());
		if (i < 255) return static_cast<uint64_t>(data.read<uint32_t>());
		return data.read<uint64_t>();
	}
}

struct Transaction {
	struct Input {
		Slice data;

		Slice hash;
		uint32_t vout;
		Slice script;
		uint32_t sequence;

		Input () {}
		Input (
			Slice data,
			Slice hash,
			uint32_t vout,
			Slice script,
			uint32_t sequence
		) :
			data(data),
			hash(hash),
			vout(vout),
			script(script),
			sequence(sequence) {}
	};

	struct Output {
		Slice data;

		Slice script;
		uint64_t value;

		Output () {}
		Output (
			Slice data,
			Slice script,
			uint64_t value
		) :
			data(data),
			script(script),
			value(value) {}
	};

	Slice data;
	int32_t version;

	std::vector<Input> inputs;
	std::vector<Output> outputs;

	uint32_t locktime;

	Transaction () {}
	Transaction (
		Slice data,
		int32_t version,
		std::vector<Input> inputs,
		std::vector<Output> outputs,
		uint32_t locktime
	) :
		data(data),
		version(version),
		inputs(inputs),
		outputs(outputs),
		locktime(locktime) {}
};

struct TransactionRange {
private:
	uint64_t n;
	Slice data;

	Transaction current;
	bool lazy = true;

	auto readTransaction () {
		const auto source = this->data;
		const auto version = this->data.read<int32_t>();

		const auto nInputs = readVI(this->data);

		std::vector<Transaction::Input> inputs;
		for (size_t i = 0; i < nInputs; ++i) {
			const auto idataSource = this->data;
			const auto hash = readSlice(this->data, 32);
			const auto vout = this->data.read<uint32_t>();

			const auto scriptLen = readVI(this->data);
			const auto script = readSlice(this->data, scriptLen);
			const auto sequence = this->data.read<uint32_t>();

			const auto idata = Slice(idataSource.begin(), this->data.begin());
			inputs.emplace_back(
				Transaction::Input(idata, hash, vout, script, sequence)
			);
		}

		const auto nOutputs = readVI(this->data);

		std::vector<Transaction::Output> outputs;
		for (size_t i = 0; i < nOutputs; ++i) {
			const auto odataSource = this->data;
			const auto value = this->data.read<uint64_t>();

			const auto scriptLen = readVI(this->data);
			const auto script = readSlice(this->data, scriptLen);

			const auto odata = Slice(odataSource.begin(), this->data.begin());
			outputs.emplace_back(Transaction::Output(odata, script, value));
		}

		const auto locktime = this->data.read<uint32_t>();
		const auto _data = source.take(source.length() - this->data.length());

		this->current = Transaction(_data, version, std::move(inputs), std::move(outputs), locktime);
		this->lazy = false;
	}

public:
	TransactionRange(size_t n, Slice data) : n(n), data(data) {}

	auto empty () const { return this->n == 0; }
	auto length () const { return this->n; }
	auto front () {
		if (this->lazy) {
			this->readTransaction();
		}

		return this->current;
	}

	// TODO: verify
// 	void popFront () {
// 		this->lazy = true;
// 		this->n--;
// 	}

	void popFront () {
		this->n--;

		if (this->n > 0) {
			this->readTransaction();
		}
	}
};

struct Block {
	Slice header, data;

	Block(Slice header) : header(header) {
		assert(header.length() == 80);
	}

	Block(Slice header, Slice data) : header(header), data(data) {
		assert(header.length() == 80);
	}

	static void calculateTarget (uint8_t* dest, uint32_t bits) {
		const auto exponent = ((bits & 0xff000000) >> 24) - 3;
		const auto mantissa = bits & 0x007fffff;
		const auto i = static_cast<size_t>(31 - exponent);
		if (i > 31) return;

		dest[i] = static_cast<uint8_t>(mantissa & 0xff);
		dest[i - 1] = static_cast<uint8_t>(mantissa >> 8);
		dest[i - 2] = static_cast<uint8_t>(mantissa >> 16);
		dest[i - 3] = static_cast<uint8_t>(mantissa >> 24);
	}

	auto bits () const {
		return this->header.peek<uint32_t>(72);
	}

	auto previousBlockHash () const {
		return this->header.drop(4).take(32);
	}

	auto transactions () const {
		auto _data = this->data;
		const auto n = readVI(_data);

		return TransactionRange(n, _data);
	}

	auto utc () const {
		return this->header.peek<uint32_t>(68);
	}

	auto verify () const {
		uint8_t hash[32];
		uint8_t target[32] = {};

		hash256(hash, this->header);
		std::reverse(hash, hash + 32);

		const auto bits = this->header.peek<uint32_t>(72);
		Block::calculateTarget(target, bits);

		return memcmp(hash, target, 32) <= 0;
	}
};
