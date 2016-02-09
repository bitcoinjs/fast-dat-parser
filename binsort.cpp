#include <parallel/algorithm>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

int main (int argc, char** argv) {
	assert(argc == 2 || argc == 3);

	size_t compareBytes = 0;
	assert(sscanf(argv[1], "%lu", &compareBytes) == 1);

	size_t dataBytes = 0;
	if (argc == 3) {
		assert(sscanf(argv[2], "%lu", &dataBytes) == 1);
	}

	const auto totalBytes = compareBytes + dataBytes;
	std::vector<uint8_t*> vector;

	// TODO: alloc into 1 huge buffer
	while (true) {
		const auto slice = new uint8_t[totalBytes];
		const auto read = fread(slice, totalBytes, 1, stdin);

		// EOF?
		if (read == 0) break;

		vector.emplace_back(slice);
	}

	__gnu_parallel::sort(
		vector.begin(), vector.end(),
		[compareBytes](const auto& a, const auto& b) {
			return memcmp(a, b, compareBytes) < 0;
		}
	);

	for (auto&& slice : vector) {
		fwrite(slice, totalBytes, 1, stdout);
	}

	return 0;
}
