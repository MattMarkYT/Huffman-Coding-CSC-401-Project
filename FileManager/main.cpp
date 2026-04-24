

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

#include <chrono>

#define FM_ENABLE_LOG true

#include "huffman/FileManager.hpp"
#include "huffman/naive.hpp"
#include "huffman/greedy.hpp"

#include "fm_tests.hpp"


const char* errorCodeToString(FMErrorCode code) {
	switch (code) {
		case FMErrorCode::none:
			return "none";
		case FMErrorCode::file_not_open:
			return "file_not_open";
		case FMErrorCode::file_open_failed:
			return "file_open_failed";
		case FMErrorCode::bad_stream_state:
			return "bad_stream_state";
		case FMErrorCode::file_read_failed:
			return "file_read_failed";
		case FMErrorCode::file_write_failed:
			return "file_write_failed";
		case FMErrorCode::file_seek_failed:
			return "file_seek_failed";
		case FMErrorCode::wrong_magic:
			return "wrong_magic";
		case FMErrorCode::wrong_type:
			return "wrong_type";
		case FMErrorCode::crc32_mismatch:
			return "crc32_mismatch";
		case FMErrorCode::wrong_emtpy_format:
			return "wrong_emtpy_format";
		case FMErrorCode::unexpected_payload_at_eof:
			return "unexpected_payload_at_eof";
		case FMErrorCode::unexpected_eof:
			return "unexpected_eof";
		case FMErrorCode::invalid_format:
			return "invalid_format";
		case FMErrorCode::invalid_dictionary_length:
			return "invalid_dictionary_length";
		case FMErrorCode::invalid_dictionary_entry:
			return "invalid_dictionary_entry";
		case FMErrorCode::invalid_dictionary_code:
			return "invalid_dictionary_code";
		case FMErrorCode::invalid_padding_bits:
			return "invalid_padding_bits";
		case FMErrorCode::invalid_payload_length:
			return "invalid_payload_length";
		case FMErrorCode::duplicate_dictionary_entry:
			return "duplicate_dictionary_entry";
		case FMErrorCode::duplicate_dictionary_code:
			return "duplicate_dictionary_code";
		case FMErrorCode::empty_dictionary:
			return "empty_dictionary";
		case FMErrorCode::null_tree:
			return "null_tree";
		case FMErrorCode::internal_error:
			return "internal_error";
		default:
			return "unknown_error";
	}
}

const char* fileTypeToString(FMFileType type) {
	switch (type) {
		case FMFileType::none:
			return "none";
		case FMFileType::plain:
			return "plain";
		case FMFileType::empty:
			return "empty";
		case FMFileType::naive:
			return "naive";
		case FMFileType::huffman:
			return "huffman";
		default:
			return "unknown";
	}
}

std::string readLine(std::string_view prompt) {
	std::cout << prompt;
	std::string line;
	std::getline(std::cin, line);
	return line;
}

bool parseMenuChoice(const std::string& text, int& outChoice) {
	if (text.empty()) {
		return false;
	}

	try {
		std::size_t consumed = 0u;
		const int value = std::stoi(text, &consumed);
		if (consumed != text.size()) {
			return false;
		}
		outChoice = value;
		return true;
	}
	catch (...) {
		return false;
	}
}

bool readFileBytes(const std::filesystem::path& path, std::vector<unsigned char>& outBytes) {
	std::ifstream file(path, std::ios::binary);
	if (!file.is_open()) {
		return false;
	}

	file.seekg(0, std::ios::end);
	const std::streamoff size = file.tellg();
	if (size < 0) {
		return false;
	}

	file.seekg(0, std::ios::beg);
	outBytes.assign(static_cast<std::size_t>(size), 0u);
	if (!outBytes.empty()) {
		file.read(reinterpret_cast<char*>(outBytes.data()), static_cast<std::streamsize>(outBytes.size()));
		if (!file) {
			return false;
		}
	}

	return true;
}


bool isTextLike(const std::vector<unsigned char>& bytes) {
	for (unsigned char byte : bytes) {
		if (byte == '\n' || byte == '\r' || byte == '\t') {
			continue;
		}
		if (!std::isprint(static_cast<unsigned char>(byte))) {
			return false;
		}
	}
	return true;
}

std::string makeTextPreview(const std::vector<unsigned char>& bytes) {
	std::string preview;
	preview.reserve(bytes.size());
	for (unsigned char byte : bytes) {
		if (byte == '\n' || byte == '\r' || byte == '\t' || std::isprint(static_cast<unsigned char>(byte))) {
			preview.push_back(static_cast<char>(byte));
		}
		else {
			preview.push_back('.');
		}
	}
	return preview;
}

void printPayload(const std::vector<unsigned char>& bytes) {
	std::cout << "Decoded payload length: " << bytes.size() << " bytes\n";

	if (bytes.empty()) {
		std::cout << "Decoded payload is empty.\n";
		return;
	}

	if (isTextLike(bytes)) {
		std::cout << "Decoded text:\n";
		std::cout << std::string(bytes.begin(), bytes.end()) << "\n";
	}
	else {
		std::cout << "Decoded text-ish preview:\n";
		std::cout << makeTextPreview(bytes) << "\n";
	}

	constexpr std::size_t hexPreviewLimit = 256u;
	const std::size_t shown = std::min(bytes.size(), hexPreviewLimit);
	std::cout << "Hex bytes";
	if (shown != bytes.size()) {
		std::cout << " (first " << shown << " of " << bytes.size() << ")";
	}
	std::cout << ":\n";
	std::cout << fmBytesToHex(std::span<const unsigned char>(bytes.data(), shown)) << "\n";
}

void printCurrentDirectory() {
	std::error_code ec;
	const std::filesystem::path cwd = std::filesystem::current_path(ec);
	if (!ec) {
		std::cout << "Current directory: " << cwd.string() << "\n";
	}
}

void encodeAndWriteFlow() {
	std::vector<unsigned char> inputBytes;

	while (true) {
		std::cout << "\nEncode and write\n";
		std::cout << "1. input text manually\n";
		std::cout << "2. load input from a file\n";
		std::cout << "3. back to main menu\n";

		int choice = 0;
		if (!parseMenuChoice(readLine("Choose input source: "), choice)) {
			std::cout << "Invalid choice.\n";
			continue;
		}

		if (choice == 1) {
			const std::string text = readLine("Enter one line of text. Empty line is allowed: ");
			inputBytes = toBytes(text);
			break;
		}

		if (choice == 2) {
			while (true) {
				printCurrentDirectory();
				const std::string fileName = readLine("Source file name (empty to go back): ");
				if (fileName.empty()) {
					break;
				}

				if (readFileBytes(fileName, inputBytes)) {
					std::cout << "Loaded " << inputBytes.size() << " bytes from file.\n";
					goto input_ready;
				}

				std::cout << "Failed to read source file. Try again.\n";
			}
			continue;
		}

		if (choice == 3) {
			return;
		}

		std::cout << "Invalid choice.\n";
	}

input_ready:

	int algorithmChoice = 0;
	if (inputBytes.empty()) {
		std::cout << "Input is empty. Empty format will be used.\n";
		std::cout << "Skipping the encoding format choice.\n";
		goto skipped_format_choice;
	}

	
	while (true) {
		std::cout << "\nChoose encoding format\n";
		std::cout << "1. naive fixed-length\n";
		std::cout << "2. huffman\n";
		std::cout << "3. back to main menu\n";

		if (!parseMenuChoice(readLine("Format: "), algorithmChoice)) {
			std::cout << "Invalid choice.\n";
			continue;
		}

		if (algorithmChoice >= 1 && algorithmChoice <= 3) {
			break;
		}

		std::cout << "Invalid choice.\n";
	}

	if (algorithmChoice == 3) {
		return;
	}


skipped_format_choice:

	printCurrentDirectory();
	const std::string outputName = readLine("Output file name without wrapper extension: ");
	if (outputName.empty()) {
		std::cout << "Cancelled.\n";
		return;
	}

	FileManager manager;
	FMErrorCode result = FMErrorCode::internal_error;
	FMFileType writtenType = FMFileType::none;


	if (inputBytes.empty()) {
		const std::unordered_map<unsigned char, std::string> emptyDictionary;
		result = manager.writeFormat(outputName, {}, 0u, emptyDictionary, true);
		writtenType = FMFileType::empty;
	}
	else if (algorithmChoice == 1) {
		writtenType = inputBytes.empty() ? FMFileType::empty : FMFileType::naive;
		std::stringstream source = makeBinaryStream(inputBytes);
		auto dictionary = createHuffmanMap(source);
		auto encodedPayload = encode(source, dictionary);
		result = manager.writeFormat(
			outputName,
			encodedPayload,
			static_cast<std::uint64_t>(inputBytes.size()),
			dictionary,
			true
		);
	}
	else {
		writtenType = inputBytes.empty() ? FMFileType::empty : FMFileType::huffman;
		std::stringstream source = makeBinaryStream(inputBytes);
		HuffNode* tree = createHuffmanTree(source);
		if (tree == nullptr) {
			std::cout << "Failed to build Huffman tree.\n";
			return;
		}

		auto encodedPayload = encode(source, tree);
		result = manager.writeFormat(
			outputName,
			encodedPayload,
			static_cast<std::uint64_t>(inputBytes.size()),
			tree,
			true
		);
		deleteTree(tree);
	}

	if (result != FMErrorCode::none) {
		std::cout << "writeFormat failed: " << errorCodeToString(result) << "\n";
		return;
	}

	std::cout << "Wrote file: " << FileManager::buildOutputFileName(outputName, writtenType) << "\n";
}

void decodedDataFlow(const std::string& stem, const std::vector<unsigned char>& decoded) {
	while (true) {
		std::cout << "\n\n1. Write decoded data to file\n";
		std::cout << "2. Display decoded data\n";

		int choice = 0;
		if (!parseMenuChoice(readLine("Choose an option: "), choice)) {
			std::cout << "Invalid choice.\n";
			continue;
		}

		if (choice == 1) {

			while (true) {
				std::cout << "1. Remove extension from the file\n";
				std::cout << "2. Write a new name for the uncompressed file\n";

				int choice = 0;
				if (!parseMenuChoice(readLine("Choose an option: "), choice)) {
					std::cout << "Invalid choice.\n";
					continue;
				}

				if (choice == 1) {
					FMErrorCode err = FileManager::writePlain(stem, decoded);

					if (err != FMErrorCode::none) {
						std::cerr << errorCodeToString(err);
						continue;
					}

					std::cout << "Done writing output to a file with removed extension." << std::endl;

					break;
				}

				if (choice == 2) {
					const std::string outputName = readLine("Output file name: ");
					if (outputName.empty()) {
						std::cout << "Cancelled.\n";
						continue;
					}
					FMErrorCode err = FileManager::writePlain(outputName, decoded);

					if (err != FMErrorCode::none) {
						std::cerr << errorCodeToString(err);
						continue;
					}
					std::cout << "Done writing output to a file with a new name." << std::endl;

					break;
				}

				std::cout << "Invalid choice.\n";
			}
			return;

		}

		if (choice == 2) {
			printPayload(decoded);
			break;
		}

		std::cout << "Invalid choice.\n";
	}

}


void decodeWrappedNaive(FileManager& manager) {
	std::unordered_map<unsigned char, std::string> dictionary;
	const FMErrorCode parseResult = manager.parseDictionary(dictionary, true);
	if (parseResult != FMErrorCode::none) {
		std::cout << "parseDictionary failed: " << errorCodeToString(parseResult) << "\n";
		return;
	}

	const FMErrorCode jumpResult = manager.jumpToData(true);
	if (jumpResult != FMErrorCode::none) {
		std::cout << "jumpToData failed: " << errorCodeToString(jumpResult) << "\n";
		return;
	}

	const std::uint64_t decodedLength = manager.getDecodedPayloadLength();

	//don't forget to get it before we detach!
	//...probably shouldn't have done that
	std::string stem = manager.getStem();


	std::shared_ptr<std::fstream> stream = manager.detachStream();
	if (stream == nullptr || !stream->is_open()) {
		std::cout << "detachStream failed.\n";
		return;
	}

	std::vector<unsigned char> decoded = decode(*stream, dictionary, static_cast<std::size_t>(decodedLength));
	
	decodedDataFlow(stem, decoded);


}

void decodeWrappedHuffman(FileManager& manager) {
	HuffNode* root = nullptr;
	const FMErrorCode parseResult = manager.parseDictionary(root, true);
	if (parseResult != FMErrorCode::none) {
		deleteTree(root);
		std::cout << "parseDictionary failed: " << errorCodeToString(parseResult) << "\n";
		return;
	}

	const FMErrorCode jumpResult = manager.jumpToData(true);
	if (jumpResult != FMErrorCode::none) {
		deleteTree(root);
		std::cout << "jumpToData failed: " << errorCodeToString(jumpResult) << "\n";
		return;
	}


	const std::string stem = manager.getStem();

	const std::uint64_t decodedLength = manager.getDecodedPayloadLength();
	std::shared_ptr<std::fstream> stream = manager.detachStream();
	if (stream == nullptr || !stream->is_open()) {
		deleteTree(root);
		std::cout << "detachStream failed.\n";
		return;
	}

	std::vector<unsigned char> decoded = decode(*stream, root, static_cast<std::size_t>(decodedLength));
	deleteTree(root);

	decodedDataFlow(stem, decoded);

	//printPayload(decoded);
}

void openAndDecodeFlow() {
	FileManager manager;
	while (true) {
		std::cout << "\nOpen file and decode\n";
		printCurrentDirectory();
		const std::string fileName = readLine("File name (empty to go back): ");
		if (fileName.empty()) {
			return;
		}

		const FMErrorCode openResult = manager.openFileR(fileName, FMFileType::none, true);
		if (openResult != FMErrorCode::none) {
			std::cout << "openFileR failed: " << errorCodeToString(openResult) << "\n";
			continue;
		}

		std::cout << "Detected file type: " << fileTypeToString(manager.getFileType()) << "\n";

		if (manager.getFileType() == FMFileType::plain) {
			std::cout << "This is a plain file. There is no custom wrapper to parse or decode.\n";
			manager.closeFile();
			return;
		}

		if (manager.getFileType() == FMFileType::empty) {
			std::cout << "Wrapped file is the special empty format.\n";
			printPayload({});
			manager.closeFile();
			return;
		}

		if (manager.getFileType() == FMFileType::naive) {
			decodeWrappedNaive(manager);
			return;
		}

		if (manager.getFileType() == FMFileType::huffman) {
			decodeWrappedHuffman(manager);
			return;
		}


		std::cout << "Unhandled file type.\n";
		manager.closeFile();
		return;
	}
}



int main() {
	
	testAllTheFMThings();

	while (true) {
		std::cout << "-------------------------------------------------------";
		std::cout << "\nMain menu\n";
		std::cout << "1. encode and write to file\n";
		std::cout << "2. open file and decode\n";
		std::cout << "3. exit\n";
	
		int choice = 0;
		if (!parseMenuChoice(readLine("Choose an option: "), choice)) {
			std::cout << "Invalid choice.\n";
			continue;
		}
	
		if (choice == 1) {
			encodeAndWriteFlow();
			continue;
		}
	
		if (choice == 2) {
			openAndDecodeFlow();
			continue;
		}
	
		if (choice == 3) {
			break;
		}
	
		std::cout << "Invalid choice.\n";
	}



	return 0;
}
