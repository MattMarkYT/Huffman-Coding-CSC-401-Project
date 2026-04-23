#ifndef FM_TESTS_HPP
#define FM_TESTS_HPP

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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

	using TestFunction = void (*)(TestRunner&, const std::filesystem::path&);

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

	void run(const std::string& name, TestFunction fn, const std::filesystem::path& testDir) {
		try {
			fn(*this, testDir);
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
	if (!in.is_open()) {
		throw TestFailure("failed to open file for reading: " + path.string());
	}

	std::vector<unsigned char> bytes{
		std::istreambuf_iterator<char>(in),
		std::istreambuf_iterator<char>()
	};

	if (in.bad()) {
		throw TestFailure("failed while reading file: " + path.string());
	}

	return bytes;
}

void writeAllBytes(const std::filesystem::path& path, const std::vector<unsigned char>& bytes) {
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	if (!out.is_open()) {
		throw TestFailure("failed to open file for writing: " + path.string());
	}

	out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	if (!out.good()) {
		throw TestFailure("failed while writing file: " + path.string());
	}

	out.flush();
	if (!out.good()) {
		throw TestFailure("failed while flushing file: " + path.string());
	}
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

std::filesystem::path makeTestStemPath(const std::filesystem::path& dir, std::string_view stem) {
	return dir / std::filesystem::path(stem);
}

void removeIfExists(const std::filesystem::path& path) {
	std::error_code ec;
	std::filesystem::remove(path, ec);
}

void removeOutputVariants(const std::filesystem::path& stemPath) {
	removeIfExists(stemPath);
	removeIfExists( FileManager::buildOutputFileName(stemPath.string(), FMFileType::naive));
	removeIfExists(FileManager::buildOutputFileName(stemPath.string(), FMFileType::huffman));
	removeIfExists(FileManager::buildOutputFileName(stemPath.string(), FMFileType::empty));
}

std::filesystem::path naiveOutputPath(const std::filesystem::path& stemPath) {
	return FileManager::buildOutputFileName(stemPath.string(), FMFileType::naive);
}

std::filesystem::path huffmanOutputPath(const std::filesystem::path& stemPath) {
	return FileManager::buildOutputFileName(stemPath.string(), FMFileType::huffman);
}

std::filesystem::path emptyOutputPath(const std::filesystem::path& stemPath) {
	return FileManager::buildOutputFileName(stemPath.string(), FMFileType::empty);
}

void checkNaiveRoundTrip(
	TestRunner& tr,
	const std::filesystem::path& dir,
	std::string_view stem,
	const std::vector<unsigned char>& payload
) {
	const std::filesystem::path stemPath = makeTestStemPath(dir, stem);
	removeOutputVariants(stemPath);

	FileManager writer;
	std::stringstream source = makeBinaryStream(payload);
	std::unordered_map<unsigned char, std::string> dictionary = createHuffmanMap(source);
	std::vector<unsigned char> encoded = encode(source, dictionary);

	tr.checkError(
		writer.writeFormat(stemPath.string(), encoded, static_cast<std::uint64_t>(payload.size()), dictionary, false),
		FMErrorCode::none,
		"naive writeFormat failed"
	);

	const std::filesystem::path outputPath = naiveOutputPath(stemPath);
	tr.check(std::filesystem::exists(outputPath), "naive output file was not created");

	FileManager reader;
	tr.checkError(
		reader.openFileR(outputPath.string(), FMFileType::none, false),
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

void checkHuffmanRoundTrip(
	TestRunner& tr,
	const std::filesystem::path& dir,
	std::string_view stem,
	const std::vector<unsigned char>& payload
) {
	const std::filesystem::path stemPath = makeTestStemPath(dir, stem);
	removeOutputVariants(stemPath);

	FileManager writer;
	std::stringstream source = makeBinaryStream(payload);
	HuffNode* tree = createHuffmanTree(source);
	tr.check(tree != nullptr, "createHuffmanTree returned nullptr");
	std::vector<unsigned char> encoded = encode(source, tree);

	tr.checkError(
		writer.writeFormat(stemPath.string(), encoded, static_cast<std::uint64_t>(payload.size()), tree, false),
		FMErrorCode::none,
		"huffman writeFormat failed"
	);
	deleteTree(tree);

	const std::filesystem::path outputPath = huffmanOutputPath(stemPath);
	tr.check(std::filesystem::exists(outputPath), "huffman output file was not created");

	FileManager reader;
	tr.checkError(
		reader.openFileR(outputPath.string(), FMFileType::none, false),
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

void testValidNaiveFileRoundTrip(TestRunner& tr, const std::filesystem::path& dir) {
	checkNaiveRoundTrip(tr, dir, "valid_naive", toBytes("Bye, naive, bye"));
}

void testValidHuffmanFileRoundTrip(TestRunner& tr, const std::filesystem::path& dir) {
	checkHuffmanRoundTrip(tr, dir, "valid_huffman", toBytes("Hello, huffman, hello!"));
}

void testEmptyFormatFile(TestRunner& tr, const std::filesystem::path& dir) {
	const std::filesystem::path stemPath = makeTestStemPath(dir, "empty_case");
	removeOutputVariants(stemPath);

	FileManager writer;
	const std::unordered_map<unsigned char, std::string> dictionary{ {'X', "0"} };
	tr.checkError(
		writer.writeFormat(stemPath.string(), std::vector<unsigned char>{}, 0u, dictionary, false),
		FMErrorCode::none,
		"writeFormat empty-case failed"
	);

	const std::filesystem::path outputPath = emptyOutputPath(stemPath);
	tr.check(std::filesystem::exists(outputPath), "empty output file was not created");

	FileManager reader;
	tr.checkError(
		reader.openFileR(outputPath.string(), FMFileType::none, false),
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
}

void testWrongMagic(TestRunner& tr, const std::filesystem::path& dir) {
	const std::filesystem::path path = dir / "wrong_magic.bin";
	removeIfExists(path);
	writeAllBytes(path, { 0x00u, 0x11u, 0x22u, 0x33u, 0x44u });

	FileManager reader;
	tr.checkError(reader.openFileR(path.string(), FMFileType::naive, false), FMErrorCode::wrong_magic, "wrong magic must be rejected");
}

void testWrongTypeNaiveAsHuffman(TestRunner& tr, const std::filesystem::path& dir) {
	const auto payload = toBytes("Bye, naive, bye");
	checkNaiveRoundTrip(tr, dir, "wrong_type_naive_source", payload);

	FileManager reader;
	tr.checkError(
		reader.openFileR(naiveOutputPath(makeTestStemPath(dir, "wrong_type_naive_source")).string(), FMFileType::huffman, false),
		FMErrorCode::wrong_type,
		"naive file opened as huffman must be rejected"
	);
}

void testWrongTypeHuffmanAsNaive(TestRunner& tr, const std::filesystem::path& dir) {
	const auto payload = toBytes("Hello, huffman, hello!");
	checkHuffmanRoundTrip(tr, dir, "wrong_type_huffman_source", payload);

	FileManager reader;
	tr.checkError(
		reader.openFileR(huffmanOutputPath(makeTestStemPath(dir, "wrong_type_huffman_source")).string(), FMFileType::naive, false),
		FMErrorCode::wrong_type,
		"huffman file opened as naive must be rejected"
	);
}

void testCRCMismatchNaive(TestRunner& tr, const std::filesystem::path& dir) {
	const auto payload = toBytes("Bye, naive, bye");
	const std::filesystem::path stemPath = makeTestStemPath(dir, "crc_mismatch_naive");
	removeOutputVariants(stemPath);

	FileManager writer;
	std::stringstream source = makeBinaryStream(payload);
	auto dictionary = createHuffmanMap(source);
	auto encoded = encode(source, dictionary);
	tr.checkError(
		writer.writeFormat(stemPath.string(), encoded, payload.size(), dictionary, false),
		FMErrorCode::none,
		"writeFormat for naive crc mismatch setup failed"
	);

	const std::filesystem::path path = naiveOutputPath(stemPath);
	auto bytes = readAllBytes(path);
	tr.check(bytes.size() >= 8u, "naive crc mismatch setup produced too-small file");

	bytes[4u] ^= 0x01u;
	writeAllBytes(path, bytes);

	const auto reread = readAllBytes(path);
	tr.check(reread.size() == bytes.size(), "naive crc mismatch rewrite changed file size unexpectedly");
	tr.check(reread[4u] == bytes[4u], "naive crc mismatch setup did not persist the CRC corruption");

	FileManager reader;
	tr.checkError(reader.openFileR(path.string(), FMFileType::none, false), FMErrorCode::crc32_mismatch, "naive crc mismatch not detected");
}

void testCRCMismatchHuffman(TestRunner& tr, const std::filesystem::path& dir) {
	const auto payload = toBytes("Hello, huffman, hello!");
	const std::filesystem::path stemPath = makeTestStemPath(dir, "crc_mismatch_huffman");
	removeOutputVariants(stemPath);

	FileManager writer;
	std::stringstream source = makeBinaryStream(payload);
	HuffNode* tree = createHuffmanTree(source);
	tr.check(tree != nullptr, "createHuffmanTree returned nullptr for huffman crc mismatch setup");
	auto encoded = encode(source, tree);
	tr.checkError(
		writer.writeFormat(stemPath.string(), encoded, payload.size(), tree, false),
		FMErrorCode::none,
		"writeFormat for huffman crc mismatch setup failed"
	);
	deleteTree(tree);

	const std::filesystem::path path = huffmanOutputPath(stemPath);
	auto bytes = readAllBytes(path);
	tr.check(bytes.size() >= 8u, "huffman crc mismatch setup produced too-small file");

	bytes[4u] ^= 0x01u;
	writeAllBytes(path, bytes);

	const auto reread = readAllBytes(path);
	tr.check(reread.size() == bytes.size(), "huffman crc mismatch rewrite changed file size unexpectedly");
	tr.check(reread[4u] == bytes[4u], "huffman crc mismatch setup did not persist the CRC corruption");

	FileManager reader;
	tr.checkError(reader.openFileR(path.string(), FMFileType::none, false), FMErrorCode::crc32_mismatch, "huffman crc mismatch not detected");
}

void testMalformedEmptyFileFormat(TestRunner& tr, const std::filesystem::path& dir) {
	const std::filesystem::path path = dir / FileManager::buildOutputFileName("bad_empty", FMFileType::empty);
	removeIfExists(path);
	writeAllBytes(path, { FileManager::magicFormat[0], FileManager::magicFormat[1], FileManager::magicFormat[2], FileManager::magicEmpty, 0x7Fu });

	FileManager reader;
	tr.checkError(reader.openFileR(path.string(), FMFileType::none, false), FMErrorCode::wrong_emtpy_format, "bad empty format not detected");
}

void testUnexpectedEOFNaive(TestRunner& tr, const std::filesystem::path& dir) {
	const std::filesystem::path path = dir / FileManager::buildOutputFileName("unexpected_eof_naive", FMFileType::naive);
	removeIfExists(path);
	auto bytes = makeNaiveWrappedBytes({ {'A', 0x00u}, {'B', 0x01u} }, 1u, 2u, { 0x00u });
	bytes.pop_back();
	patchCRC32C(bytes);
	writeAllBytes(path, bytes);

	FileManager reader;
	tr.checkError(reader.openFileR(path.string(), FMFileType::none, false), FMErrorCode::unexpected_eof, "naive unexpected EOF not detected");
}

void testUnexpectedEOFHuffman(TestRunner& tr, const std::filesystem::path& dir) {
	const std::filesystem::path path = dir / FileManager::buildOutputFileName("unexpected_eof_huffman", FMFileType::huffman);
	removeIfExists(path);
	auto bytes = makeHuffmanWrappedBytes(
		{
			{'A', 0u, {0x00u}},
			{'B', 0u, {0x01u}}
		},
		1u,
		1u,
		{ 0x00u }
	);
	bytes.pop_back();
	patchCRC32C(bytes);
	writeAllBytes(path, bytes);

	FileManager reader;
	tr.checkError(reader.openFileR(path.string(), FMFileType::none, false), FMErrorCode::unexpected_eof, "huffman unexpected EOF not detected");
}

void testUnexpectedPayloadAtEOFNaive(TestRunner& tr, const std::filesystem::path& dir) {
	const std::filesystem::path path = dir / FileManager::buildOutputFileName("unexpected_payload_naive", FMFileType::naive);
	removeIfExists(path);
	auto bytes = makeNaiveWrappedBytes({ {'A', 0x00u}, {'B', 0x01u} }, 1u, 2u, { 0x00u });
	bytes.push_back(0xAAu);
	patchCRC32C(bytes);
	writeAllBytes(path, bytes);

	FileManager reader;
	tr.checkError(reader.openFileR(path.string(), FMFileType::none, false), FMErrorCode::unexpected_payload_at_eof, "naive unexpected payload at EOF not detected");
}

void testUnexpectedPayloadAtEOFHuffman(TestRunner& tr, const std::filesystem::path& dir) {
	const std::filesystem::path path = dir / FileManager::buildOutputFileName("unexpected_payload_huffman", FMFileType::huffman);
	removeIfExists(path);
	auto bytes = makeHuffmanWrappedBytes(
		{
			{'A', 0u, {0x00u}},
			{'B', 0u, {0x01u}}
		},
		1u,
		1u,
		{ 0x00u }
	);
	bytes.push_back(0xAAu);
	patchCRC32C(bytes);
	writeAllBytes(path, bytes);

	FileManager reader;
	tr.checkError(reader.openFileR(path.string(), FMFileType::none, false), FMErrorCode::unexpected_payload_at_eof, "huffman unexpected payload at EOF not detected");
}

void testDuplicateDictionaryEntryFailureNaive(TestRunner& tr, const std::filesystem::path& dir) {
	const std::filesystem::path path = dir / FileManager::buildOutputFileName("duplicate_symbol_naive", FMFileType::naive);
	removeIfExists(path);
	writeAllBytes(path, makeNaiveWrappedBytes({ {'A', 0x00u}, {'A', 0x01u} }, 1u, 1u, { 0x00u }));

	FileManager reader;
	tr.checkError(reader.openFileR(path.string(), FMFileType::none, false), FMErrorCode::none, "open naive duplicate symbol file failed structurally");
	std::unordered_map<unsigned char, std::string> parsed;
	tr.checkError(reader.parseDictionary(parsed, false), FMErrorCode::duplicate_dictionary_entry, "naive duplicate symbol not rejected semantically");
}

void testDuplicateDictionaryEntryFailureHuffman(TestRunner& tr, const std::filesystem::path& dir) {
	const std::filesystem::path path = dir / FileManager::buildOutputFileName("duplicate_symbol_huffman", FMFileType::huffman);
	removeIfExists(path);
	writeAllBytes(
		path,
		makeHuffmanWrappedBytes(
			{
				{'A', 0u, {0x00u}},
				{'A', 0u, {0x01u}}
			},
			1u,
			1u,
			{ 0x00u }
		)
	);

	FileManager reader;
	tr.checkError(reader.openFileR(path.string(), FMFileType::none, false), FMErrorCode::none, "open huffman duplicate symbol file failed structurally");
	HuffNode* parsed = nullptr;
	const FMErrorCode parseResult = reader.parseDictionary(parsed, false);
	deleteTree(parsed);
	tr.checkError(parseResult, FMErrorCode::duplicate_dictionary_entry, "huffman duplicate symbol not rejected semantically");
}

void testDuplicateCodeFailureNaive(TestRunner& tr, const std::filesystem::path& dir) {
	const std::filesystem::path path = dir / FileManager::buildOutputFileName("duplicate_code_naive", FMFileType::naive);
	removeIfExists(path);
	writeAllBytes(path, makeNaiveWrappedBytes({ {'A', 0x00u}, {'B', 0x00u} }, 1u, 1u, { 0x00u }));

	FileManager reader;
	tr.checkError(reader.openFileR(path.string(), FMFileType::none, false), FMErrorCode::none, "open naive duplicate code file failed structurally");
	std::unordered_map<unsigned char, std::string> parsed;
	tr.checkError(reader.parseDictionary(parsed, false), FMErrorCode::duplicate_dictionary_code, "naive duplicate code not rejected semantically");
}

void testDuplicateCodeFailureHuffman(TestRunner& tr, const std::filesystem::path& dir) {
	const std::filesystem::path path = dir / FileManager::buildOutputFileName("duplicate_code_huffman", FMFileType::huffman);
	removeIfExists(path);
	writeAllBytes(
		path,
		makeHuffmanWrappedBytes(
			{
				{'A', 0u, {0x00u}},
				{'B', 0u, {0x00u}}
			},
			1u,
			1u,
			{ 0x00u }
		)
	);

	FileManager reader;
	tr.checkError(reader.openFileR(path.string(), FMFileType::none, false), FMErrorCode::none, "open huffman duplicate code file failed structurally");
	HuffNode* parsed = nullptr;
	const FMErrorCode parseResult = reader.parseDictionary(parsed, false);
	deleteTree(parsed);
	tr.checkError(parseResult, FMErrorCode::duplicate_dictionary_code, "huffman duplicate code not rejected semantically");
}

void testInvalidHuffmanTreeConflict(TestRunner& tr, const std::filesystem::path& dir) {
	const std::filesystem::path path = dir / FileManager::buildOutputFileName("huffman_prefix_conflict", FMFileType::huffman);
	removeIfExists(path);
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
	tr.check(
		parseResult == FMErrorCode::invalid_dictionary_code || parseResult == FMErrorCode::duplicate_dictionary_code,
		"huffman prefix conflict not rejected semantically"
	);
}

void testInvalidEncodedDataSizeNaive(TestRunner& tr, const std::filesystem::path& dir) {
	const std::filesystem::path path = dir / FileManager::buildOutputFileName("invalid_payload_length_naive", FMFileType::naive);
	removeIfExists(path);
	auto bytes = makeNaiveWrappedBytes({ {'A', 0x00u}, {'B', 0x01u} }, 2u, 1u, { 0x00u });
	patchCRC32C(bytes);
	writeAllBytes(path, bytes);

	FileManager reader;
	tr.checkError(reader.openFileR(path.string(), FMFileType::none, false), FMErrorCode::invalid_payload_length, "naive invalid payload length not detected");
}

void testMixtureOfTextAndBinaryPayloads(TestRunner& tr, const std::filesystem::path& dir) {
	const auto payload = makeBinaryMixturePayload();
	checkNaiveRoundTrip(tr, dir, "mix_naive", payload);
	checkHuffmanRoundTrip(tr, dir, "mix_huffman", payload);
}

void testRandomByteSequencesNaive(TestRunner& tr, const std::filesystem::path& dir) {
	for (const std::size_t length : { 64u, 256u, 1024u, 10240u }) {
		const auto payload = generateRandomBytes(length);
		checkNaiveRoundTrip(tr, dir, "rnd_naive_" + std::to_string(length), payload);
	}
}

void testRandomByteSequencesHuffman(TestRunner& tr, const std::filesystem::path& dir) {
	for (const std::size_t length : { 64u, 256u, 1024u, 10240u }) {
		const auto payload = generateRandomBytes(length);
		checkHuffmanRoundTrip(tr, dir, "rnd_huffman_" + std::to_string(length), payload);
	}
}

bool testAllTheFMThings() {
	std::srand(12345);

	const std::filesystem::path testDir = std::filesystem::current_path() / "file_manager_tests";
	std::filesystem::create_directories(testDir);

	TestRunner tr;

	tr.run("valid naive file round-trip", testValidNaiveFileRoundTrip, testDir);
	tr.run("valid huffman file round-trip", testValidHuffmanFileRoundTrip, testDir);
	tr.run("empty-format file", testEmptyFormatFile, testDir);
	tr.run("wrong magic", testWrongMagic, testDir);
	tr.run("wrong type naive file opened as huffman", testWrongTypeNaiveAsHuffman, testDir);
	tr.run("wrong type huffman file opened as naive", testWrongTypeHuffmanAsNaive, testDir);
	tr.run("CRC mismatch naive", testCRCMismatchNaive, testDir);
	tr.run("CRC mismatch huffman", testCRCMismatchHuffman, testDir);
	tr.run("malformed empty-file format", testMalformedEmptyFileFormat, testDir);
	tr.run("unexpected EOF naive", testUnexpectedEOFNaive, testDir);
	tr.run("unexpected EOF huffman", testUnexpectedEOFHuffman, testDir);
	tr.run("unexpected payload at EOF naive", testUnexpectedPayloadAtEOFNaive, testDir);
	tr.run("unexpected payload at EOF huffman", testUnexpectedPayloadAtEOFHuffman, testDir);
	tr.run("duplicate dictionary entry failure in parseDictionary naive", testDuplicateDictionaryEntryFailureNaive, testDir);
	tr.run("duplicate dictionary entry failure in parseDictionary huffman", testDuplicateDictionaryEntryFailureHuffman, testDir);
	tr.run("duplicate code failure in parseDictionary naive", testDuplicateCodeFailureNaive, testDir);
	tr.run("duplicate code failure in parseDictionary huffman", testDuplicateCodeFailureHuffman, testDir);
	tr.run("invalid Huffman tree conflict in parseDictionary", testInvalidHuffmanTreeConflict, testDir);
	tr.run("invalid encoded data size naive", testInvalidEncodedDataSizeNaive, testDir);
	tr.run("mixture of text and binary payloads", testMixtureOfTextAndBinaryPayloads, testDir);
	tr.run("random byte sequences of length 64, 256, and 1024 naive", testRandomByteSequencesNaive, testDir);
	tr.run("random byte sequences of length 64, 256, and 1024 huffman", testRandomByteSequencesHuffman, testDir);

	std::cout << "\nTotal passed: " << tr.passed << "\n";
	std::cout << "Total failed: " << tr.failed << "\n";

	return !tr.failed;
}


#endif // !FM_TESTS_HPP
