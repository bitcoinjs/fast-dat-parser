#pragma once

static const unsigned char HEX_TABLE[] = {
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	0,1,2,3,4,5,6,7,8,9, // 0-9
	255,255,255,255,255,255,255,
	10,11,12,13,14,15, // a-f
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	10,11,12,13,14,15, // A-F
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
};

static const char HEX_ALPHABET[] = "0123456789abcdef";

// destination needs atleast length/2 bytes
inline int hex_decode (unsigned char* destination, const char* source, const size_t length) {
	if (length % 2 != 0) return 0;

	for (size_t i = 0; i < length; i += 2) {
		const unsigned char a = HEX_TABLE[static_cast<unsigned char>(source[i])];
		const unsigned char b = HEX_TABLE[static_cast<unsigned char>(source[i + 1])];
		if (a == 255) return 0;
		if (b == 255) return 0;

		destination[i >> 1] = static_cast<unsigned char>((a << 4) + b);
	}

	return 1;
}

// destination needs atleast length*2 bytes
inline int hex_encode (char* destination, const unsigned char* source, const size_t length) {
	for (size_t i = 0; i < length; ++i) {
		const unsigned char x = source[i];
		const int a = x >> 4;
		const int b = x & 0x0f;

		destination[i << 1] = HEX_ALPHABET[a];
		destination[(i << 1) + 1] = HEX_ALPHABET[b];
	}

	return 1;
}
