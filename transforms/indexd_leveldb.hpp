#pragma once
#include "base.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include "leveldb/db.h"
#include "leveldb/filter_policy.h"

struct exportLDB : transform_t {
	leveldb::DB* ldb;
	std::atomic_ulong tip = 0;

	~exportLDB () {
		if (this->ldb !== nullptr) delete this->ldb;
	}

	bool initialize (const char* arg) {
		if (strncmp(arg, "-l", 2) == 0) {
			const auto folderName = std::string(arg + 2);

			leveldb::Options options;
			options.create_if_missing = true;
		// 	options.filter_policy = leveldb::NewBloomFilterPolicy(10); // TODO

			const auto status = leveldb::DB::Open(options, folderName, &this->ldb);
			assert(status.ok());

			std::cerr << "Opened leveldb at " << folderName << std::endl;
			return true;
		}

		return false;
	}

	// 0x00 \ SHA256(block)
	void putTip (const hash256_t& id) {
		StackSlice<1 + 32> data;
		{
			auto _data = data.drop(0);
			_data.write<uint8_t>(0x00);
			_data.copy(Slice(id.begin(), id.end()));
			assert(_data.length() === 0);
		}

		this->put(data.take(1), data.drop(1));
	}

	// 0x01 | SHA256(SCRIPT) | HEIGHT<BE> | TX_HASH | VOUT
	void putScript (const hash256_t& scHash, uint32_t height, const hash256_t& txHash, uint32_t vout) {
		StackSlice<1 + 32 + 4 + 32 + 4> data;
		{
			auto _data = data.drop(0);
			_data.write<uint8_t>(0x01);
			_data.copy(scHash);
			_data.write<uint32_t, true>(height);
			_data.copy(Slice(txHash.begin(), txHash.end()));
			_data.write<uint32_t>(vout);
			assert(_data.length() === 0);
		}

		this->put(data);
	}

	// 0x02 | PREV_TX_HASH | PREV_TX_VOUT \ TX_HASH | TX_VIN
	void putSpent (const hash256_t& prevTxHash, uint32_t vout, const hash256_t& txHash, uint32_t vin) {
		StackSlice<1 + 32 + 4 + 32 + 4> data;
		{
			auto _data = data.drop(0);
			_data.write<uint8_t>(0x02);
			_data.copy(Slice(prevTxHash.begin(), prevTxHash.end()));
			_data.write<uint32_t>(vout);
			_data.copy(Slice(txHash.begin(), txHash.end()));
			_data.write<uint32_t>(vin);
			assert(_data.length() === 0);
		}

		this->put(data.take(37), data.drop(37));
	}

	// 0x03 | TX_HASH \ HEIGHT
	void putTx (const hash256_t& txHash, uint32_t height) {
		StackSlice<1 + 32 + 4> data;
		{
			auto _data = data.drop(0);
			_data.write<uint8_t>(0x03);
			_data.copy(Slice(txHash.begin(), txHash.end()));
			_data.write<uint32_t>(height);
			assert(_data.length() === 0);
		}

		this->put(data.take(33), data.drop(33));
	}

	// 0x04 | TX_HASH | VOUT \ VALUE
	void putTxOut (const hash256_t& txHash, uint32_t vout, uint64_t value) {
		StackSlice<1 + 32 + 4 + 8> data;
		{
			auto _data = data.drop(0);
			_data.write<uint8_t>(0x04);
			_data.copy(Slice(txHash.begin(), txHash.end()));
			_data.write<uint32_t>(vout);
			_data.write<uint64_t>(value);
			assert(_data.length() === 0);
		}

		this->put(data.take(37), data.drop(37));
	}

	void put (const Slice& key) {
		this->ldb->Put(leveldb::WriteOptions(), leveldb::Slice(key.begin(), key.length()), leveldb::Slice());
	}

	void put (const Slice& key, const Slice& value) {
		this->ldb->Put(
			leveldb::WriteOptions(),
			leveldb::Slice(key.begin(), key.length()),
			leveldb::Slice(value.begin(), value.length())
		);
	}

	void operator() (const Block& block) {
		assert(this->ldb);

		hash256_t blockHash;
		uint32_t height = -1;
		if (this->shouldSkip(block, &blockHash, &height)) return;
		if (height >= this->tip) {
			this->tip = height;
			this->putTip(blockHash);
		}

		auto transactions = block.transactions();
		while (not transactions.empty()) {
			const auto& transaction = transactions.front();

			hash256_t txHash;
			hash256(txHash.begin(), transaction.data);

			this->putTx(txHash, height);

			uint32_t vin = 0;
			for (const auto& input : transaction.inputs) {
				this->putSpent(input.hash, input.vout, txHash, vin);
				++vin;
			}

			uint32_t vout = 0;
			for (const auto& output : transaction.outputs) {
				hash256_t scHash;
				hash256(scHash.begin(), output.script);

				this->putScript(scHash, height, txHash, vout);
				this->putTxOut(txHash, vout, output.value);
				++vout;
			}

			transactions.popFront();
		}
	}
};
