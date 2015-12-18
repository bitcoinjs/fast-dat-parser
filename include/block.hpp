#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>

#include "hash.hpp"
#include "slice.hpp"

uint64_t readVI (Slice& data) {
	const auto i = data.peek();
	data.popFront();

	if (i < 253) return static_cast<uint64_t>(i);
	if (i < 254) return static_cast<uint64_t>(data.read<uint16_t>());
	if (i < 255) return static_cast<uint64_t>(data.read<uint32_t>());
	return data.read<uint64_t>();
}

struct Transaction {
	struct Input {
		Slice hash;
		uint32_t vout;
		Slice script;
		uint32_t sequence;

		Input () {}
		Input (
			Slice hash,
			uint32_t vout,
			Slice script,
			uint32_t sequence
		) :
			hash(hash),
			vout(vout),
			script(script),
			sequence(sequence) {}
	};

	struct Output {
		Slice script;
		uint64_t value;

		Output () {}
		Output (
			Slice script,
			uint64_t value
		) :
			script(script),
			value(value) {}
	};

	Slice data;
	uint32_t version;

	std::vector<Input> inputs;
	std::vector<Output> outputs;

	uint32_t locktime;

	Transaction () {}
	Transaction (
		Slice data,
		uint32_t version,
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

namespace {
	auto readSlice (Slice& data, uint64_t n) {
		auto slice = data.take(n);
		data.popFrontN(n);
		return slice;
	}
}

struct TransactionRange {
private:
	uint64_t n;
	Slice data;

	Transaction current;
	bool lazy = true;

	auto readTransaction () {
		const auto source = this->data;
		const auto version = this->data.read<uint32_t>();

		auto&& inputs = this->current.inputs;
		inputs.resize(readVI(this->data));

		for (auto &input : inputs) {
			auto hash = readSlice(this->data, 32);
			auto vout = this->data.read<uint32_t>();
			auto script = readSlice(this->data, readVI(this->data));
			auto sequence = this->data.read<uint32_t>();

			input = Transaction::Input(hash, vout, script, sequence);
		}

		auto&& outputs = this->current.outputs;
		outputs.resize(readVI(this->data));

		for (auto &output : outputs) {
			auto value = this->data.read<uint64_t>();
			auto script = readSlice(this->data, readVI(this->data));

			output = Transaction::Output(script, value);
		}

		auto locktime = this->data.read<uint32_t>();
		auto tdata = source.take(source.length() - this->data.length());

		this->current = Transaction(tdata, version, std::move(inputs), std::move(outputs), locktime);
		this->lazy = false;
	}

public:
	TransactionRange(size_t n, Slice data) : n(n), data(data) {}

	auto empty () const { return this->n == 0; }
	auto length () const { return this->n; }
	auto& front () {
		if (this->lazy) {
			this->readTransaction();
		}

		return this->current;
	}

	void popFront () {
		this->n--;

		if (this->n > 0) {
			this->readTransaction();
		}
	}
};

struct Block {
	Slice header;
	Slice data;

	Block(Slice header) : header(header) {
		assert(header.length() == 80);
	}

	Block(Slice header, Slice data) : header(header), data(data) {
		assert(header.length() == 80);
	}

	void target (uint8_t buffer[32]) const {
		const auto bits = *(reinterpret_cast<uint32_t*>(this->header.begin + 72));
		const uint32_t exponent = ((bits & 0xff000000) >> 24) - 3;
		const uint32_t mantissa = bits & 0x007fffff;
		const size_t i = static_cast<size_t>(31 - exponent);
		if (i > 32) return;

		buffer[i] = static_cast<uint8_t>(mantissa & 0xff);
		buffer[i - 1] = static_cast<uint8_t>(mantissa >> 8);
		buffer[i - 2] = static_cast<uint8_t>(mantissa >> 16);
		buffer[i - 3] = static_cast<uint8_t>(mantissa >> 24);
	}

	auto transactions () const {
		auto n = readVI(this->data);

		return TransactionRange(n, this->data);
	}

	auto verify () const {
		uint8_t hash[32];
		uint8_t _target[32] = {};

		hash256(&hash[0], this->header);
		std::reverse(&hash[0], &hash[32]);
		this->target(_target);

		return memcmp(hash, _target, 32) <= 0;
	}
};
