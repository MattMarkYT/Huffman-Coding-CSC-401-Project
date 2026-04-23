#ifndef FM_TESTS_HPP
#define FM_TESTS_HPP

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <istream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "huffman/FileManager.hpp"
#include "huffman/naive.hpp"
#include "huffman/greedy.hpp"


struct TestFailure : std::runtime_error {
	using std::runtime_error::runtime_error;
};

struct TestRunner {
	int passed{ 0 };
	int failed{ 0 };

	void check(bool condition, const std::string& message) {
		if (!condition) {
			throw TestFailure(message);
		}
	}

	void checkError(FMErrorCode actual, FMErrorCode expected, const std::string& message) {
		if (actual != expected) {
			throw TestFailure(message + " expected=" + std::to_string(static_cast<int>(expected))
				+ " actual=" + std::to_string(static_cast<int>(actual)));
		}
	}

	void run(const std::string& name, const auto& fn) {
		try {
			fn();
			std::cout << "------------------------------------------\n";
			std::cout << "PASS: " << name << '\n';
			std::cout << "------------------------------------------\n";
			++passed;
		}
		catch (const std::exception& ex) {
			std::cout << "==========================================\n";
			std::cout << "FAIL: " << name << " -- " << ex.what() << '\n';
			std::cout << "==========================================\n";
			++failed;
		}
	}
};

std::vector<unsigned char> makeBinaryMixturePayload() {
	return {
		0x00u, 0x41u, 0x42u, 0x43u, 0xFFu, 0x10u, 0x0Au, 0x7Fu,
		0x20u, 0x21u, 0x80u, 0x81u, 0x01u, 0x02u, 0x03u, 0x04u,
		0x42u, 0x00u, 0x99u, 0x5Au, 0x0Du, 0x0Au, 0xC4u, 0xFEu
	};
}

std::vector<unsigned char> generateRandomBytes(std::size_t length) {
	std::vector<unsigned char> bytes(length);
	for (std::size_t index = 0u; index < length; ++index) {
		bytes[index] = static_cast<unsigned char>(std::rand() % 256);
	}
	return bytes;
}


std::vector<unsigned char> readAllBytes(const std::filesystem::path& path) {
	std::ifstream in(path, std::ios::binary);
	return std::vector<unsigned char>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void writeAllBytes(const std::filesystem::path& path, const std::vector<unsigned char>& bytes) {
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

std::uint32_t calcCRC32C(const std::vector<unsigned char>& bytes, std::size_t startOffset) {
	std::uint32_t crc = 0xFFFFFFFFu;
	for (std::size_t index = startOffset; index < bytes.size(); ++index) {
		crc ^= static_cast<std::uint32_t>(bytes[index]);
		for (int bit = 0; bit < 8; ++bit) {
			const bool lowBit = (crc & 1u) != 0u;
			crc >>= 1u;
			if (lowBit) {
				crc ^= FileManager::crc32cPolynomial;
			}
		}
	}
	return crc ^ 0xFFFFFFFFu;
}

void patchCRC32C(std::vector<unsigned char>& bytes) {
	if (bytes.size() < 8u) {
		return;
	}
	const std::uint32_t crc = calcCRC32C(bytes, 8u);
	const auto crcBytes = fmU32ToLE(crc);
	for (std::size_t index = 0u; index < crcBytes.size(); ++index) {
		bytes[4u + index] = crcBytes[index];
	}
}

std::vector<unsigned char> makeNaiveWrappedBytes(
	const std::vector<std::pair<unsigned char, unsigned char>>& entries,
	std::uint64_t encodedPayloadLength,
	std::uint64_t decodedPayloadLength,
	const std::vector<unsigned char>& payload
) {
	std::vector<unsigned char> bytes;
	bytes.push_back(FileManager::magicFormat[0]);
	bytes.push_back(FileManager::magicFormat[1]);
	bytes.push_back(FileManager::magicFormat[2]);
	bytes.push_back(FileManager::magicNaive);
	bytes.insert(bytes.end(), 4u, 0u);
	bytes.push_back(static_cast<unsigned char>(entries.size() - 1u));
	for (const auto& [symbol, codeByte] : entries) {
		bytes.push_back(symbol);
		bytes.push_back(codeByte);
	}
	const auto encodedLengthBytes = fmU64ToLE(encodedPayloadLength);
	const auto decodedLengthBytes = fmU64ToLE(decodedPayloadLength);
	bytes.insert(bytes.end(), encodedLengthBytes.begin(), encodedLengthBytes.end());
	bytes.insert(bytes.end(), decodedLengthBytes.begin(), decodedLengthBytes.end());
	bytes.insert(bytes.end(), payload.begin(), payload.end());
	patchCRC32C(bytes);
	return bytes;
}

std::vector<unsigned char> makeHuffmanWrappedBytes(
	const std::vector<std::tuple<unsigned char, unsigned char, std::vector<unsigned char>>>& entries,
	std::uint64_t encodedPayloadLength,
	std::uint64_t decodedPayloadLength,
	const std::vector<unsigned char>& payload
) {
	std::vector<unsigned char> bytes;
	bytes.push_back(FileManager::magicFormat[0]);
	bytes.push_back(FileManager::magicFormat[1]);
	bytes.push_back(FileManager::magicFormat[2]);
	bytes.push_back(FileManager::magicHuffman);
	bytes.insert(bytes.end(), 4u, 0u);
	bytes.push_back(static_cast<unsigned char>(entries.size() - 1u));
	for (const auto& [symbol, bitLengthMinusOne, codeBytes] : entries) {
		bytes.push_back(symbol);
		bytes.push_back(bitLengthMinusOne);
		bytes.insert(bytes.end(), codeBytes.begin(), codeBytes.end());
	}
	const auto encodedLengthBytes = fmU64ToLE(encodedPayloadLength);
	const auto decodedLengthBytes = fmU64ToLE(decodedPayloadLength);
	bytes.insert(bytes.end(), encodedLengthBytes.begin(), encodedLengthBytes.end());
	bytes.insert(bytes.end(), decodedLengthBytes.begin(), decodedLengthBytes.end());
	bytes.insert(bytes.end(), payload.begin(), payload.end());
	patchCRC32C(bytes);
	return bytes;
}

void checkVectorEqual(TestRunner& tr, const std::vector<unsigned char>& actual, const std::vector<unsigned char>& expected, const std::string& where) {
	tr.check(actual == expected, where + " payload mismatch");
}

void checkNaiveRoundTrip(TestRunner& tr, const std::filesystem::path& dir, std::string_view stem, const std::vector<unsigned char>& payload) {
	FileManager writer;
	std::stringstream source = makeBinaryStream(payload);
	std::unordered_map<unsigned char, std::string> dictionary = createHuffmanMap(source);
	std::vector<unsigned char> encoded = encode(source, dictionary);

	tr.checkError(
		writer.writeFormat((dir / std::filesystem::path(stem)).string(), encoded, static_cast<std::uint64_t>(payload.size()), dictionary, false),
		FMErrorCode::none,
		"naive writeFormat failed"
	);

	FileManager reader;
	tr.checkError(
		reader.openFileR((dir / std::filesystem::path(FileManager::buildOutputFileName(stem, FMFileType::naive))).string(), FMFileType::none, false),
		FMErrorCode::none,
		"naive openFileR failed"
	);
	tr.check(reader.getFileType() == FMFileType::naive, "naive detected wrong file type");
	tr.check(reader.getEncodedPayloadLength() == encoded.size(), "naive cached encoded length mismatch");
	tr.check(reader.getDecodedPayloadLength() == payload.size(), "naive cached decoded length mismatch");
	tr.check(reader.getDictLength() == dictionary.size(), "naive cached dictionary length mismatch");

	std::unordered_map<unsigned char, std::string> parsedDictionary;
	tr.checkError(reader.parseDictionary(parsedDictionary, false), FMErrorCode::none, "naive parseDictionary failed");
	tr.checkError(reader.jumpToData(false), FMErrorCode::none, "naive jumpToData failed");
	const std::uint64_t decodedLength = reader.getDecodedPayloadLength();
	std::shared_ptr<std::fstream> detached = reader.detachStream();
	std::vector<unsigned char> decoded = decode(*detached, parsedDictionary, static_cast<std::size_t>(decodedLength));
	checkVectorEqual(tr, decoded, payload, "naive round-trip");
}

void checkHuffmanRoundTrip(TestRunner& tr, const std::filesystem::path& dir, std::string_view stem, const std::vector<unsigned char>& payload) {
	FileManager writer;
	std::stringstream source = makeBinaryStream(payload);
	HuffNode* tree = createHuffmanTree(source);
	tr.check(tree != nullptr, "createHuffmanTree returned nullptr");
	std::vector<unsigned char> encoded = encode(source, tree);

	tr.checkError(
		writer.writeFormat((dir / stem).string(), encoded, static_cast<std::uint64_t>(payload.size()), tree, false),
		FMErrorCode::none,
		"huffman writeFormat failed"
	);
	deleteTree(tree);

	FileManager reader;
	tr.checkError(
		reader.openFileR((dir / std::filesystem::path(FileManager::buildOutputFileName(stem, FMFileType::huffman))).string(), FMFileType::none, false),
		FMErrorCode::none,
		"huffman openFileR failed"
	);
	tr.check(reader.getFileType() == FMFileType::huffman, "huffman detected wrong file type");
	tr.check(reader.getEncodedPayloadLength() == encoded.size(), "huffman cached encoded length mismatch");
	tr.check(reader.getDecodedPayloadLength() == payload.size(), "huffman cached decoded length mismatch");

	HuffNode* parsedTree = nullptr;
	tr.checkError(reader.parseDictionary(parsedTree, false), FMErrorCode::none, "huffman parseDictionary failed");
	tr.checkError(reader.jumpToData(false), FMErrorCode::none, "huffman jumpToData failed");
	const std::uint64_t decodedLength = reader.getDecodedPayloadLength();
	std::shared_ptr<std::fstream> detached = reader.detachStream();
	std::vector<unsigned char> decoded = decode(*detached, parsedTree, static_cast<std::size_t>(decodedLength));
	deleteTree(parsedTree);
	checkVectorEqual(tr, decoded, payload, "huffman round-trip");
}




bool testAllTheFMThings() {
	std::srand(12345);

	const std::filesystem::path testDir = std::filesystem::current_path() / "file_manager_test_files";
	std::filesystem::create_directories(testDir);

	TestRunner tr;

	tr.run("valid naive file round-trip", [&]() {
		checkNaiveRoundTrip(tr, testDir, "valid_naive", toBytes("Bye, naive, bye"));
		});

	tr.run("valid huffman file round-trip", [&]() {
		checkHuffmanRoundTrip(tr, testDir, "valid_huffman", toBytes("Hello, huffman, hello!"));
		});

	tr.run("empty-format file", [&]() {
		FileManager writer;
		const std::unordered_map<unsigned char, std::string> dictionary{ {'X', "0"} };
		tr.checkError(
			writer.writeFormat((testDir / "empty_case").string(), std::vector<unsigned char>{}, 0u, dictionary, false),
			FMErrorCode::none,
			"writeFormat empty-case failed"
		);

		FileManager reader;
		tr.checkError(
			reader.openFileR((testDir / FileManager::buildOutputFileName("empty_case", FMFileType::empty)).string(), FMFileType::none, false),
			FMErrorCode::none,
			"openFileR empty-case failed"
		);
		tr.check(reader.getFileType() == FMFileType::empty, "empty-case wrong detected type");
		tr.check(reader.getCRC() == 0u, "empty-case crc must be zero");
		tr.check(reader.getDictLength() == 0u, "empty-case dict length must be zero");
		tr.check(reader.getEncodedPayloadLength() == 0u, "empty-case encoded length must be zero");
		tr.check(reader.getDecodedPayloadLength() == 0u, "empty-case decoded length must be zero");
		std::unordered_map<unsigned char, std::string> parsed;
		tr.checkError(reader.parseDictionary(parsed, false), FMErrorCode::wrong_type, "empty-case parseDictionary must reject empty");
		tr.checkError(reader.jumpToData(false), FMErrorCode::wrong_type, "empty-case jumpToData must reject empty");
		});

	tr.run("wrong magic", [&]() {
		const std::filesystem::path path = testDir / "wrong_magic.bin";
		writeAllBytes(path, { 0x00u, 0x11u, 0x22u, 0x33u, 0x44u });
		FileManager reader;
		tr.checkError(reader.openFileR(path.string(), FMFileType::naive, false), FMErrorCode::wrong_magic, "wrong magic must be rejected");
		});

	tr.run("wrong type", [&]() {
		const auto payload = toBytes("Bye, naive, bye");
		checkNaiveRoundTrip(tr, testDir, "wrong_type_source", payload);
		FileManager reader;
		tr.checkError(
			reader.openFileR((testDir / FileManager::buildOutputFileName("wrong_type_source", FMFileType::naive)).string(), FMFileType::huffman, false),
			FMErrorCode::wrong_type,
			"wrong type must be rejected"
		);
		});

	tr.run("CRC mismatch", [&]() {
		const auto payload = toBytes("Bye, naive, bye");
		FileManager writer;
		std::stringstream source = makeBinaryStream(payload);
		auto dictionary = createHuffmanMap(source);
		auto encoded = encode(source, dictionary);
		tr.checkError(
			writer.writeFormat((testDir / "crc_mismatch").string(), encoded, payload.size(), dictionary, false),
			FMErrorCode::none,
			"writeFormat for crc mismatch setup failed"
		);
		const std::filesystem::path path = testDir / FileManager::buildOutputFileName("crc_mismatch", FMFileType::naive);
		auto bytes = readAllBytes(path);
		tr.check(!bytes.empty(), "crc mismatch setup produced empty file");
		bytes.back() ^= 0x01u;
		writeAllBytes(path, bytes);
		FileManager reader;
		tr.checkError(reader.openFileR(path.string(), FMFileType::none, false), FMErrorCode::crc32_mismatch, "crc mismatch not detected");
		});

	tr.run("malformed empty-file format", [&]() {
		const std::filesystem::path path = testDir / "bad_empty.hemt";
		writeAllBytes(path, { FileManager::magicFormat[0], FileManager::magicFormat[1], FileManager::magicFormat[2], FileManager::magicEmpty, 0x7Fu });
		FileManager reader;
		tr.checkError(reader.openFileR(path.string(), FMFileType::none, false), FMErrorCode::wrong_emtpy_format, "bad empty format not detected");
		});

	tr.run("unexpected EOF", [&]() {
		const std::filesystem::path path = testDir / FileManager::buildOutputFileName("unexpected_eof", FMFileType::naive);
		auto bytes = makeNaiveWrappedBytes({ {'A', 0x00u}, {'B', 0x01u} }, 1u, 2u, { 0x00u });
		bytes.pop_back();
		patchCRC32C(bytes);
		writeAllBytes(path, bytes);
		FileManager reader;
		tr.checkError(reader.openFileR(path.string(), FMFileType::none, false), FMErrorCode::unexpected_eof, "unexpected EOF not detected");
		});

	tr.run("unexpected payload at EOF", [&]() {
		const std::filesystem::path path = testDir / FileManager::buildOutputFileName("unexpected_payload", FMFileType::naive);
		auto bytes = makeNaiveWrappedBytes({ {'A', 0x00u}, {'B', 0x01u} }, 1u, 2u, { 0x00u });
		bytes.push_back(0xAAu);
		patchCRC32C(bytes);
		writeAllBytes(path, bytes);
		FileManager reader;
		tr.checkError(reader.openFileR(path.string(), FMFileType::none, false), FMErrorCode::unexpected_payload_at_eof, "unexpected payload at EOF not detected");
		});

	tr.run("duplicate dictionary entry failure in parseDictionary", [&]() {
		const std::filesystem::path path = testDir / FileManager::buildOutputFileName("duplicate_symbol", FMFileType::naive);
		writeAllBytes(path, makeNaiveWrappedBytes({ {'A', 0x00u}, {'A', 0x01u} }, 1u, 1u, { 0x00u }));
		FileManager reader;
		tr.checkError(reader.openFileR(path.string(), FMFileType::none, false), FMErrorCode::none, "open duplicate symbol file failed structurally");
		std::unordered_map<unsigned char, std::string> parsed;
		tr.checkError(reader.parseDictionary(parsed, false), FMErrorCode::duplicate_dictionary_entry, "duplicate symbol not rejected semantically");
		});

	tr.run("duplicate code failure in parseDictionary", [&]() {
		const std::filesystem::path path = testDir / FileManager::buildOutputFileName("duplicate_code", FMFileType::naive);
		writeAllBytes(path, makeNaiveWrappedBytes({ {'A', 0x00u}, {'B', 0x00u} }, 1u, 1u, { 0x00u }));
		FileManager reader;
		tr.checkError(reader.openFileR(path.string(), FMFileType::none, false), FMErrorCode::none, "open duplicate code file failed structurally");
		std::unordered_map<unsigned char, std::string> parsed;
		tr.checkError(reader.parseDictionary(parsed, false), FMErrorCode::duplicate_dictionary_code, "duplicate code not rejected semantically");
		});

	tr.run("invalid Huffman tree conflict in parseDictionary", [&]() {
		const std::filesystem::path path = testDir / FileManager::buildOutputFileName("huffman_prefix_conflict", FMFileType::huffman);
		writeAllBytes(
			path,
			makeHuffmanWrappedBytes(
				{
					{'A', 0u, {0x00u}},
					{'B', 1u, {0x00u}}
				},
				1u,
				1u,
				{ 0x00u }
			)
		);
		FileManager reader;
		tr.checkError(reader.openFileR(path.string(), FMFileType::none, false), FMErrorCode::none, "open huffman conflict file failed structurally");
		HuffNode* parsed = nullptr;
		const FMErrorCode parseResult = reader.parseDictionary(parsed, false);
		deleteTree(parsed);
		tr.check(parseResult == FMErrorCode::invalid_dictionary_code || parseResult == FMErrorCode::duplicate_dictionary_code, "huffman prefix conflict not rejected semantically");
		});

	tr.run("invalid encoded data size", [&]() {
		const std::filesystem::path path = testDir / FileManager::buildOutputFileName("invalid_payload_length", FMFileType::naive);
		auto bytes = makeNaiveWrappedBytes({ {'A', 0x00u}, {'B', 0x01u} }, 2u, 1u, { 0x00u });
		patchCRC32C(bytes);
		writeAllBytes(path, bytes);
		FileManager reader;
		tr.checkError(reader.openFileR(path.string(), FMFileType::none, false), FMErrorCode::invalid_payload_length, "invalid payload length not detected");
		});

	tr.run("mixture of text and binary payloads", [&]() {
		const auto payload = makeBinaryMixturePayload();
		checkNaiveRoundTrip(tr, testDir, "mix_naive", payload);
		checkHuffmanRoundTrip(tr, testDir, "mix_huffman", payload);
		});

	tr.run("random byte sequences of length 64, 256, and 1024", [&]() {
		for (const std::size_t length : {64u, 256u, 1024u}) {
			const auto payload = generateRandomBytes(length);
			checkNaiveRoundTrip(tr, testDir, "rnd_naive_" + std::to_string(length), payload);
			checkHuffmanRoundTrip(tr, testDir, "rnd_huffman_" + std::to_string(length), payload);
		}
		});


	std::cout << "\nTotal passed: " << tr.passed << "\n";
	std::cout << "Total failed: " << tr.failed << "\n";

	return !tr.failed;

}


#endif // !FM_TESTS_HPP

