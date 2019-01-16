#pragma once

#include <atomic>
#include <iostream>
#include <iomanip>
#include <leveldb/db.h>
#include <leveldb/filter_policy.h>
#include <leveldb/write_batch.h>

#include "hash.hpp"
#include "ranger.hpp"
#include "serial.hpp"
#include "transforms.hpp"
using namespace ranger;

namespace {
	template <typename R>
	void put (leveldb::WriteBatch& batch, const R& key, const R& value) {
		batch.Put(
			leveldb::Slice(reinterpret_cast<const char*>(key.begin()), key.size()),
			leveldb::Slice(reinterpret_cast<const char*>(value.begin()), value.size())
		);
	}

	// 0x00 \ BLOCK_HASH
	void putTip (leveldb::WriteBatch& batch, const uint256_t& id) {
		std::array<uint8_t, 1 + 32> data;
		{
			auto _data = range(data);
			serial::put<uint8_t>(_data, 0x00);
			_data.put(retro(id));
			assert(_data.size() == 0);
		}

		put(batch, range(data).take(1), range(data).drop(1));
	}

	// 0x01 | SHA256(SCRIPT) | HEIGHT<BE> | TX_HASH | VOUT
	template <typename S>
	void putScript (leveldb::WriteBatch& batch, const S& script, const uint32_t height, const uint256_t& txHash, const uint32_t vout) {
		std::array<uint8_t, 1 + 32 + 4 + 32 + 4> data;
		{
			const auto scHash = sha256(script);
			auto _data = range(data);
			serial::put<uint8_t>(_data, 0x01);
			_data.put(scHash);
			serial::put<uint32_t, true>(_data, height); // big-endian for indexing
			_data.put(retro(txHash));
			serial::put<uint32_t>(_data, vout);
			assert(_data.size() == 0);
		}

		put(batch, range(data), range(data).take(0));
	}

	// 0x02 | PREV_TX_HASH | PREV_TX_VOUT \ TX_HASH | TX_VIN
	template <typename S>
	void putSpent (leveldb::WriteBatch& batch, const S& prevTxHash, const uint32_t vout, const uint256_t& txHash, const uint32_t vin) {
		std::array<uint8_t, 1 + 32 + 4 + 32 + 4> data;
		{
			auto _data = range(data);
			serial::put<uint8_t>(_data, 0x02);
			_data.put(retro(prevTxHash));
			serial::put<uint32_t>(_data, vout);
			_data.put(retro(txHash));
			serial::put<uint32_t>(_data, vin);
			assert(_data.size() == 0);
		}

		put(batch, range(data).take(37), range(data).drop(37));
	}

	// 0x03 | TX_HASH \ HEIGHT
	void putTx (leveldb::WriteBatch& batch, const uint256_t& txHash, const uint32_t height) {
		std::array<uint8_t, 1 + 32 + 4> data;
		{
			auto _data = range(data);
			serial::put<uint8_t>(_data, 0x03);
			_data.put(retro(txHash));
			serial::put<uint32_t>(_data, height);
			assert(_data.size() == 0);
		}

		put(batch, range(data).take(33), range(data).drop(33));
	}

	// 0x04 | TX_HASH | VOUT \ VALUE | SCRIPT
	void putTxo (leveldb::WriteBatch& batch, const uint256_t& txHash, const uint32_t vout, const uint64_t value) {
		std::array<uint8_t, 1 + 32 + 4 + 8> data;
		{
			auto _data = range(data);
			serial::put<uint8_t>(_data, 0x04);
			_data.put(retro(txHash));
			serial::put<uint32_t>(_data, vout);
			serial::put<uint64_t>(_data, value);
			// TODO: missing script
			assert(_data.size() == 0); // wont pass
		}

		put(batch, range(data).take(37), range(data).drop(37));
	}
}

template <typename Block>
struct dumpIndexdLevel : public TransformBase<Block> {
	leveldb::DB* ldb;
	std::atomic_ulong maxHeight;

	dumpIndexdLevel () : maxHeight(0) {}
	~dumpIndexdLevel () {
		if (this->ldb != nullptr) delete this->ldb;
	}

	bool initialize (const char* arg) {
		if (TransformBase<Block>::initialize(arg)) return true;
		if (strncmp(arg, "-l", 2) == 0) {
			const auto folderName = std::string(arg + 2);

			leveldb::Options options;
			options.create_if_missing = true;
			options.error_if_exists = true;
			options.compression = leveldb::kNoCompression;
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

		uint256_t blockHash;
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
			const auto txHash = transaction.hash();

			putTx(batch, txHash, height);

			uint32_t vin = 0;
			for (const auto& input : transaction.inputs) {
				putSpent(batch, input.hash, input.vout, txHash, vin);
				++vin;
			}

			uint32_t vout = 0;
			for (const auto& output : transaction.outputs) {
				putScript(batch, output.script, height, txHash, vout);
				putTxo(batch, txHash, vout, output.value);
				++vout;
			}

			transactions.popFront();
		}

		const auto status = this->write(batch);
		assert(status.ok());
	}
};
