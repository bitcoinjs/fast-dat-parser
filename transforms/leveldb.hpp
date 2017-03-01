#pragma once
#include "base.hpp"
#include <iostream>
#include <iomanip>
#include "leveldb/db.h"
#include "leveldb/filter_policy.h"
#include "leveldb/write_batch.h"

namespace {
	void put (leveldb::WriteBatch& batch, const Slice& key, const Slice& value) {
		batch.Put(
			leveldb::Slice(reinterpret_cast<const char*>(key.begin()), key.length()),
			leveldb::Slice(reinterpret_cast<const char*>(value.begin()), value.length())
		);
	}

	// 0x00 \ BLOCK_HASH
	void putTip (leveldb::WriteBatch& batch, const hash256_t& id) {
		StackSlice<1 + 32> data;
		{
			auto _data = data.drop(0);
			_data.write<uint8_t>(0x00);
			_data.writeN(id.begin(), 32);
			assert(_data.length() == 0);
		}

		put(batch, data.take(1), data.drop(1));
	}

	// 0x01 | SHA256(SCRIPT) | HEIGHT<BE> | TX_HASH | VOUT
	void putScript (leveldb::WriteBatch& batch, const hash256_t& scHash, uint32_t height, const hash256_t& txHash, uint32_t vout) {
		StackSlice<1 + 32 + 4 + 32 + 4> data;
		{
			auto _data = data.drop(0);
			_data.write<uint8_t>(0x01);
			_data.writeN(scHash.begin(), 32);
			_data.write<uint32_t, true>(height); // big-endian for indexing
			_data.writeN(txHash.begin(), 32);
			_data.write<uint32_t>(vout);
			assert(_data.length() == 0);
		}

		put(batch, data.drop(0), data.take(0));
	}

	// 0x02 | PREV_TX_HASH | PREV_TX_VOUT \ TX_HASH | TX_VIN
	void putSpent (leveldb::WriteBatch& batch, const Slice& prevTxHash, uint32_t vout, const hash256_t& txHash, uint32_t vin) {
		StackSlice<1 + 32 + 4 + 32 + 4> data;
		{
			auto _data = data.drop(0);
			_data.write<uint8_t>(0x02);
			_data.writeN(prevTxHash.begin(), 32);
			_data.write<uint32_t>(vout);
			_data.writeN(txHash.begin(), 32);
			_data.write<uint32_t>(vin);
			assert(_data.length() == 0);
		}

		put(batch, data.take(37), data.drop(37));
	}

	// 0x03 | TX_HASH \ HEIGHT
	void putTx (leveldb::WriteBatch& batch, const hash256_t& txHash, uint32_t height) {
		StackSlice<1 + 32 + 4> data;
		{
			auto _data = data.drop(0);
			_data.write<uint8_t>(0x03);
			_data.writeN(txHash.begin(), 32);
			_data.write<uint32_t>(height);
			assert(_data.length() == 0);
		}

		put(batch, data.take(33), data.drop(33));
	}

	// 0x04 | TX_HASH | VOUT \ VALUE
	void putTxOut (leveldb::WriteBatch& batch, const hash256_t& txHash, uint32_t vout, uint64_t value) {
		StackSlice<1 + 32 + 4 + 8> data;
		{
			auto _data = data.drop(0);
			_data.write<uint8_t>(0x04);
			_data.writeN(txHash.begin(), 32);
			_data.write<uint32_t>(vout);
			_data.write<uint64_t>(value);
			assert(_data.length() == 0);
		}

		put(batch, data.take(37), data.drop(37));
	}
}

struct dumpIndexdLevel : public transform_t {
	leveldb::DB* ldb;
	std::atomic_ulong maxHeight;

	dumpIndexdLevel () {
		this->maxHeight = 0;
	}

	~dumpIndexdLevel () {
		if (this->ldb != nullptr) delete this->ldb;
	}

	bool initialize (const char* arg) {
		if (transform_t::initialize(arg)) return true;
		if (strncmp(arg, "-l", 2) == 0) {
			const auto folderName = std::string(arg + 2);

			leveldb::Options options;
			options.create_if_missing = true;
			options.error_if_exists = true;
			options.write_buffer_size = 1 * 1024 * 1024 * 1024; // 1 GiB
		// 	options.filter_policy = leveldb::NewBloomFilterPolicy(10); // TODO

			const auto status = leveldb::DB::Open(options, folderName, &this->ldb);
			assert(status.ok());

			std::cerr << "Opened leveldb at " << folderName << std::endl;
			return true;
		}

		return false;
	}

	auto write (leveldb::WriteBatch& batch) {
		return this->ldb->Write(leveldb::WriteOptions(), &batch);
	}

	void operator() (const Block& block) {
		assert(this->ldb);
		assert(not this->whitelist.empty());

		hash256_t blockHash;
		uint32_t height = 0xffffffff;
		if (this->shouldSkip(block, &blockHash, &height)) return;
		assert(height != 0xffffffff);

		leveldb::WriteBatch batch;
		if (height >= this->maxHeight) {
			this->maxHeight = height;
			putTip(batch, blockHash);
		}

		auto transactions = block.transactions();
		while (not transactions.empty()) {
			const auto& transaction = transactions.front();

			hash256_t txHash;
			hash256(txHash.begin(), transaction.data);

			putTx(batch, txHash, height);

			uint32_t vin = 0;
			for (const auto& input : transaction.inputs) {
				putSpent(batch, input.hash, input.vout, txHash, vin);
				++vin;
			}

			uint32_t vout = 0;
			for (const auto& output : transaction.outputs) {
				hash256_t scHash;
				hash256(scHash.begin(), output.script);

				putScript(batch, scHash, height, txHash, vout);
				putTxOut(batch, txHash, vout, output.value);
				++vout;
			}

			transactions.popFront();
		}

		const auto status = this->write(batch);
		assert(status.ok());
	}
};
