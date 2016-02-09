#include <parallel/algorithm>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

int main (int argc, char** argv) {
	assert(argc == 2 || argc == 3 || argc == 4);

	size_t compareOffset = 0;
	size_t compareBytes = 0;
	size_t totalBytes = 0;

	// eg. 32, compare bytes[0:32], 32 total bytes
	if (argc == 2) {
		assert(sscanf(argv[1], "%lu", &compareBytes) == 1);
		totalBytes = compareBytes;
	}

	// eg. 20 40, compare bytes[0:20], 40 total bytes
	if (argc == 3) {
		assert(sscanf(argv[1], "%lu", &compareBytes) == 1);
		assert(sscanf(argv[2], "%lu", &totalBytes) == 1);
	}

	// eg. 2 22 22, compare bytes[2:22], 22 total bytes
	if (argc == 4) {
		assert(sscanf(argv[1], "%lu", &compareOffset) == 1);
		assert(sscanf(argv[2], "%lu", &compareBytes) == 1);
		assert(compareBytes >= compareOffset);
		compareBytes = compareBytes - compareOffset;
		assert(sscanf(argv[3], "%lu", &totalBytes) == 1);
	}

	assert(compareBytes > 0);
	assert(compareOffset < totalBytes);

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
		[=](const auto& a, const auto& b) {
			return memcmp(a + compareOffset, b + compareOffset, compareBytes) < 0;
		}
	);

	for (auto&& slice : vector) {
		fwrite(slice, totalBytes, 1, stdout);
	}

	return 0;
}
