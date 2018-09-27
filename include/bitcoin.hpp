#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

#include "hash.hpp"
#include "hexxer.hpp"
#include "ranger.hpp"
#include "serial.hpp"
#include "bitcoin-ops.hpp"

template <typename R>
struct TransactionBase {
	struct Input {
		R data;
		R hash;
		uint32_t vout;
		R script;
		std::vector<R> scriptStack;
		uint32_t sequence;
		uint8_t witnessFlag; // OP_NOWITNESS means not a witness program
	};

	struct Output {
		R data;
		R script;
		uint64_t value;
	};

	struct Witness {
		R data;
		std::vector<R> stack;
	};

	R data;
	int32_t version;
	std::vector<Input> inputs;
	std::vector<Output> outputs;
	std::vector<Witness> witnesses;
	uint32_t locktime;

	auto hash () const {
		return hash256(this->data);
	}
};

namespace {
	template <typename R>
	auto readRange (R& r, size_t n) {
		auto v = r.take(n);
		r.popFrontN(n);
		return v;
	}

	template <typename R>
	auto readVI (R& r) {
		const auto i = serial::read<uint8_t>(r);
		if (i < 253) return static_cast<uint64_t>(i);
		if (i < 254) return static_cast<uint64_t>(serial::read<uint16_t>(r));
		if (i < 255) return static_cast<uint64_t>(serial::read<uint32_t>(r));
		return serial::read<uint64_t>(r);
	}

	template <typename R>
	auto readVI (R&& r) { return readVI<R>(r); }

	template <typename R>
	uint32_t readPD (const uint8_t opcode, R& range) {
		if (opcode < OP_PUSHDATA1) return opcode;
		if (opcode == OP_PUSHDATA1) return serial::read<uint8_t>(range);
		if (opcode == OP_PUSHDATA2) return serial::read<uint16_t>(range);
		assert(opcode == OP_PUSHDATA4);
		return serial::read<uint32_t>(range);
	}

	template <typename R>
	auto readPD (const uint8_t opcode, R&& r) { return readPD<R>(opcode, r); }

	template <typename R>
	auto readStack (R& r) {
		const auto count = readVI(r);

		std::vector<R> stack;
		for (uint64_t i = 0; i < count; ++i) {
			stack.emplace_back(readRange(r, readVI(r)));
		}

		return std::move(stack);
	}

	template <typename R>
	auto readTransaction (R& data) {
		using Transaction = TransactionBase<R>;

		auto save = data;
		const auto version = serial::read<int32_t>(data);

		// segregated witness
		const auto marker = serial::peek<uint8_t>(data);
		const auto flag = serial::peek<uint8_t>(data.drop(1));
		const auto hasWitnesses = (marker == 0x00) && (flag == 0x01);
		if (hasWitnesses) data.popFrontN(2);

		const auto nInputs = readVI(data);
		std::vector<typename Transaction::Input> inputs;

		for (size_t i = 0; i < nInputs; ++i) {
			auto isave = data;
			const auto hash = readRange(data, 32);
			const auto vout = serial::read<uint32_t>(data);

			const auto scriptLen = readVI(data);

			__ranger::Range scriptRanger(data.begin(), data.begin() + scriptLen);

			std::vector<R> scriptStack;

			while(scriptRanger.size()) {

				auto opcode = serial::read<uint8_t>(scriptRanger);

				if(!opcode || opcode > OP_PUSHDATA4)
					continue;

				auto size = readPD(opcode, scriptRanger);

				scriptStack.push_back(readRange(scriptRanger, size));
			}

			const auto script = readRange(data, scriptLen);
			const auto sequence = serial::read<uint32_t>(data);
			isave.popBackN(data.size());

			uint8_t witnessFlag = OP_NOWITNESS;

			if(hasWitnesses) {

				if(script.size() == 0) {

					// In this case we must analyze witness data to determine the witness type.
					// We'll check for OP_ERROR during witness data processing and set the type there.

					witnessFlag = OP_ERROR;
				}
				else if(script.size() >= 3) {

					// As per https://github.com/bitcoin/bips/blob/master/bip-0141.mediawiki#witness-program

					if(script[0] > 2 && script[0] < 40 && script[1] == 0) {

						if(script[2] == 0x14)
							witnessFlag |= OP_P2WPKH;
						else if(script[2] == 0x20)
							witnessFlag |= OP_P2WSH;
						else
							witnessFlag = OP_ERROR;
					}
				}
			}
			
			inputs.emplace_back(typename Transaction::Input{isave, hash, vout, script, scriptStack, sequence, witnessFlag});
		}

		const auto nOutputs = readVI(data);
		std::vector<typename Transaction::Output> outputs;

		for (size_t i = 0; i < nOutputs; ++i) {
			auto osave = data;
			const auto value = serial::read<uint64_t>(data);
			const auto scriptLen = readVI(data);
			const auto script = readRange(data, scriptLen);
			osave.popBackN(data.size());

			outputs.emplace_back(typename Transaction::Output{osave, script, value});
		}

		std::vector<typename Transaction::Witness> witnesses;
		if (hasWitnesses) {
			for (size_t i = 0; i < nInputs; ++i) {
				auto wsave = data;
				const auto stack = readStack(data);
				wsave.popBackN(data.size());

				witnesses.emplace_back(typename Transaction::Witness{wsave, std::move(stack)});

				if(inputs[i].witnessFlag == OP_ERROR && !inputs[i].script.size()) {

					// As stated in https://github.com/bitcoin/bips/blob/master/bip-0141.mediawiki#witness-program
					//
					// If the version byte is 0, and the witness program is 20 bytes:
					// It is interpreted as a pay-to-witness-public-key-hash (P2WPKH) program.
					// The witness must consist of exactly 2 items (≤ 520 bytes each). The first one a signature, and the second one a public key.

					// We will check for exactly 2 items in witness stack to conform to above

					if(witnesses.back().stack.size() == 2)
						inputs[i].witnessFlag = OP_P2WPKH;
					else if(witnesses.back().stack.size() > 2)
						inputs[i].witnessFlag = OP_P2WSH;
				}
			}
		}

		const auto locktime = serial::read<uint32_t>(data);
		save.popBackN(data.size());

		return Transaction{save, version, std::move(inputs), std::move(outputs), std::move(witnesses), locktime};
	}

	template <typename R>
	auto readTransaction (R&& data) { return readTransaction<R>(data); }
}

template <typename R>
struct TransactionRange {
private:
	size_t _count;
	R _data;
	R _save;

public:
	TransactionRange (R data, size_t count) : _count(count), _data(data), _save(data.take(0)) {}

	auto empty () const { return this->_count == 0; }
	auto size () const { return this->_count; }
	auto front () {
		this->_save = this->_data;
		return readTransaction(this->_save);
	}

	void popFront () {
		assert(!this->empty());
		--this->_count;

		if (this->_save.empty()) {
			readTransaction(this->_data);
			return;
		}

		this->_data = this->_save;
	}
};

template <typename S>
struct BlockBase {
	S header;
	S data;

	BlockBase (S header, S data) : header(header), data(data) {}

	static void calculateTarget (uint256_t& target, uint32_t bits) {
		const auto exponent = ((bits & 0xff000000) >> 24) - 3;
		const auto mantissa = bits & 0x007fffff;
		const auto i = static_cast<size_t>(28 - exponent);
		if (i > 28) return;

		serial::place<uint32_t, true>(range(target).drop(i), mantissa);
	}

	auto bits () const {
		return serial::peek<uint32_t>(this->header.drop(72));
	}

	auto hash () const {
		return hash256(this->header);
	}

	auto previousBlockHash () const {
		return this->header.drop(4).take(32);
	}

	auto transactions () const {
		auto copy = this->data;
		const auto count = readVI(copy);
		return TransactionRange<S>(copy, count);
	}

	auto utc () const {
		return serial::peek<uint32_t>(this->header.drop(68));
	}

	auto verify () const {
		auto hash = this->hash();
		std::reverse(hash.begin(), hash.end());

		uint256_t target = {};
		BlockBase::calculateTarget(target, this->bits());

		return memcmp(hash.data(), target.data(), target.size()) <= 0;
	}
};

template <typename R>
auto Block (const R& header, const R& data) {
	return BlockBase<R>(header, data);
}

// TODO: output can max-out ...
template <typename R>
void putASM (R& output, const R& script) {
	auto save = range(script);

	while (not save.empty()) {
		const auto opcode = serial::read<uint8_t>(save);

		// data
		if ((opcode > OP_0) && (opcode <= OP_PUSHDATA4)) {
			const auto dataLength = readPD(opcode, save);
			if (dataLength > save.size()) {
				for (auto x : zstr_range("<ERROR>")) {
					serial::put<char>(output, x);
				}
				return;
			}

			const auto data = save.take(dataLength);
			save.popFrontN(dataLength);

			putHex(output, data);
			serial::put<char>(output, ' ');

		// opcode
		} else {
			for (auto x : zstr_range(getOpString(opcode))) {
				serial::put<char>(output, x);
			}
			serial::put<char>(output, ' ');
		}
	}
}
