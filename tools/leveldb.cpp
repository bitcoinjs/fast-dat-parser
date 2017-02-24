#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include "leveldb/db.h"
#include "leveldb/filter_policy.h"

auto initDatabase (const std::string fileName) {
	leveldb::DB* db = nullptr;
    leveldb::Options options;
    options.create_if_missing = true;
// 	options.filter_policy = leveldb::NewBloomFilterPolicy(10); // TODO

    const auto status = leveldb::DB::Open(options, fileName, &db);
    if (status.ok()) return db;

	std::cerr << "Failed to open/create database " << fileName << std::endl;
	std::cerr << status.ToString() << std::endl;
	return static_cast<leveldb::DB*>(nullptr);
}

int main () {
	auto db = initDatabase("./ldbindex");

	while (true) {
		uint8_t buffer[32];
		const auto read = fread(buffer, sizeof(buffer), 1, stdin);

		// EOF?
		if (read == 0) break;

        std::ostringstream keyStream;
        std::ostringstream valueStream;

        keyStream << "key " << 1;
        valueStream << "value " << 1;

		leveldb::WriteOptions writeOptions;
        db->Put(writeOptions, keyStream.str(), valueStream.str());
	}

	delete db;
	return 0;
}
