#ifndef FM_MISC_HPP
#define FM_MISC_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <span>
#include <string>
#include "huffman/HuffNode.hpp"

// This gives the byte count needed to store a bit count.
inline constexpr std::size_t fmBitCountToByteCount(std::size_t bitCount) noexcept {
	return bitCount == 0u ? 0u : ((bitCount - 1u) / 8u) + 1u;
}




// This mirrors max(1, ceil(log2(dictionary_length))) without floating point.
inline constexpr std::size_t fmNaiveCodeWidth(std::size_t dictionaryLength) noexcept {
	if (dictionaryLength <= 1u) {
		return 1u;
	}

	std::size_t width = 0u;
	std::size_t value = dictionaryLength - 1u;
	while (value != 0u) {
		++width;
		value >>= 1u;
	}

	return width == 0u ? 1u : width;
}

// This reads a payload-style bit where bit index 0 is the leftmost bit in the byte.
inline constexpr bool fmGetBitMsb(unsigned char value, std::size_t bitIndex) noexcept {
	if (bitIndex >= 8u) {
		return false;
	}

	return ((value >> (7u - bitIndex)) & 0x01u) != 0u;
}

// This writes a payload-style bit where bit index 0 is the leftmost bit in the byte.
inline constexpr void fmSetBitMsb(unsigned char& value, std::size_t bitIndex, bool bit) noexcept {
	if (bitIndex >= 8u) {
		return;
	}

	const unsigned char mask = static_cast<unsigned char>(1u << (7u - bitIndex));
	if (bit) {
		value = static_cast<unsigned char>(value | mask);
		return;
	}

	value = static_cast<unsigned char>(value & static_cast<unsigned char>(~mask));
}

// This packs a 32-bit integer to little-endian bytes.
inline constexpr std::array<unsigned char, 4> fmU32ToLE(std::uint32_t value) noexcept {
	return {
		static_cast<unsigned char>(value & 0xFFu),
		static_cast<unsigned char>((value >> 8u) & 0xFFu),
		static_cast<unsigned char>((value >> 16u) & 0xFFu),
		static_cast<unsigned char>((value >> 24u) & 0xFFu)
	};
}

// This packs a 64-bit integer to little-endian bytes.
inline constexpr std::array<unsigned char, 8> fmU64ToLE(std::uint64_t value) noexcept {
	return {
		static_cast<unsigned char>(value & 0xFFu),
		static_cast<unsigned char>((value >> 8u) & 0xFFu),
		static_cast<unsigned char>((value >> 16u) & 0xFFu),
		static_cast<unsigned char>((value >> 24u) & 0xFFu),
		static_cast<unsigned char>((value >> 32u) & 0xFFu),
		static_cast<unsigned char>((value >> 40u) & 0xFFu),
		static_cast<unsigned char>((value >> 48u) & 0xFFu),
		static_cast<unsigned char>((value >> 56u) & 0xFFu)
	};
}

// This unpacks a 32-bit little-endian byte sequence.
inline constexpr std::uint32_t fmLEToU32(std::span<const unsigned char, 4> bytes) noexcept {
	return static_cast<std::uint32_t>(bytes[0])
		| (static_cast<std::uint32_t>(bytes[1]) << 8u)
		| (static_cast<std::uint32_t>(bytes[2]) << 16u)
		| (static_cast<std::uint32_t>(bytes[3]) << 24u);
}

// This unpacks a 64-bit little-endian byte sequence.
inline constexpr std::uint64_t fmLEToU64(std::span<const unsigned char, 8> bytes) noexcept {
	return static_cast<std::uint64_t>(bytes[0])
		| (static_cast<std::uint64_t>(bytes[1]) << 8u)
		| (static_cast<std::uint64_t>(bytes[2]) << 16u)
		| (static_cast<std::uint64_t>(bytes[3]) << 24u)
		| (static_cast<std::uint64_t>(bytes[4]) << 32u)
		| (static_cast<std::uint64_t>(bytes[5]) << 40u)
		| (static_cast<std::uint64_t>(bytes[6]) << 48u)
		| (static_cast<std::uint64_t>(bytes[7]) << 56u);
}

// This is just for log strings like DE AD BE EF.
inline std::string fmBytesToHex(std::span<const unsigned char> bytes) {
	static constexpr char hexDigits[] = "0123456789ABCDEF";

	if (bytes.empty()) {
		return {};
	}

	std::string text;
	text.reserve((bytes.size() * 3u) - 1u);
	for (std::size_t index = 0u; index < bytes.size(); ++index) {
		if (index != 0u) {
			text.push_back(' ');
		}

		text.push_back(hexDigits[(bytes[index] >> 4u) & 0x0Fu]);
		text.push_back(hexDigits[bytes[index] & 0x0Fu]);
	}

	return text;
}



// This makes the log output less noisy when we only want one byte in hex.
inline std::string hexByte(unsigned char value) {
	const std::array<unsigned char, 1> bytes{ value };
	return fmBytesToHex(bytes);
}

// This is a tiny helper for exact leaf checks on the uploaded HuffNode type.
inline bool isLeaf(const HuffNode* node) {
	return node != nullptr && node->left == nullptr && node->right == nullptr;
}

// This keeps the naive payload-length validation readable and overflow-safe.
inline std::uint64_t expectedNaivePayloadBytes(std::uint64_t decodedLength, std::size_t codeWidth) {
	const std::uint64_t fullGroups = decodedLength / 8u;
	const std::uint64_t remainder = decodedLength % 8u;
	return (fullGroups * static_cast<std::uint64_t>(codeWidth))
		+ ((remainder * static_cast<std::uint64_t>(codeWidth) + 7u) / 8u);
}



inline std::stringstream makeBinaryStream(const std::vector<unsigned char>& bytes) {
	std::stringstream stream(std::ios::in | std::ios::out | std::ios::binary);
	const std::string data(bytes.begin(), bytes.end());
	stream.write(data.data(), static_cast<std::streamsize>(data.size()));
	stream.clear();
	stream.seekg(0, std::ios::beg);
	stream.seekp(0, std::ios::beg);
	return stream;
}


inline std::vector<unsigned char> toBytes(std::string_view text) {
	return std::vector<unsigned char>(text.begin(), text.end());
}

#endif
