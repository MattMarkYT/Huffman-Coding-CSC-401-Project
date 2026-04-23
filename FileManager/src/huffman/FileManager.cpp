
#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "huffman/FileManager.hpp"
#include "huffman/FMMisc.hpp"


void FileManager::resetState() {
	m_fileName.clear();
	m_fileType = FMFileType::none;
	m_crc32 = 0u;
	m_dictPos = invalidPosition;
	m_dictLength = 0u;
	m_payloadPos = invalidPosition;
	m_encodedPayloadLength = 0u;
	m_decodedPayloadLength = 0u;
}

FMErrorCode FileManager::openFileR(
	std::string_view fileName,
	FMFileType expectedType,
	bool logging
) {
	closeFile();

	if (!m_file) {
		m_file = std::make_shared<std::fstream>();
	}

	m_fileName = std::string(fileName);
	m_file->open(m_fileName, std::ios::binary | std::ios::in);
	if (!m_file->is_open()) {
		resetState();
		return FMErrorCode::file_open_failed;
	}

	if (!m_file->good()) {
		closeFile();
		return FMErrorCode::bad_stream_state;
	}

	std::array<unsigned char, 4> magic{};
	std::size_t readCount = 0u;
	m_file->read(reinterpret_cast<char*>(magic.data()), static_cast<std::streamsize>(magic.size()));
	if (m_file->gcount() > 0) {
		readCount = static_cast<std::size_t>(m_file->gcount());
	}

	m_file->clear();
	m_file->seekg(0, std::ios::beg);
	if (!m_file->good()) {
		closeFile();
		return FMErrorCode::file_seek_failed;
	}

	const FMFileType detectedType = (readCount == magic.size())
		? detectTypeFromMagic(magic)
		: FMFileType::plain;

	if (logging || FM_FORCE_LOG) {
		std::cout << "openFileR: magic bytes: "
			<< fmBytesToHex(std::span<const unsigned char>(magic.data(), readCount))
			<< '\n';
		std::cout << "openFileR: detected type: " << typeName(detectedType) << '\n';
	}

	if (expectedType != FMFileType::none) {
		if (expectedType == FMFileType::plain) {
			if (detectedType != FMFileType::plain) {
				closeFile();
				return FMErrorCode::wrong_type;
			}

			m_fileType = FMFileType::plain;
			if (logging || FM_FORCE_LOG) {
				std::cout << "openFileR: expected type matched: plain\n";
			}
			return FMErrorCode::none;
		}

		if (readCount != magic.size()) {
			closeFile();
			return FMErrorCode::unexpected_eof;
		}

		const bool familyMatches = std::equal(
			magic.begin(),
			magic.begin() + static_cast<std::ptrdiff_t>(magicFormat.size()),
			magicFormat.begin()
		);
		if (!familyMatches) {
			closeFile();
			return FMErrorCode::wrong_magic;
		}

		if (detectedType != expectedType) {
			closeFile();
			return FMErrorCode::wrong_type;
		}

		m_fileType = expectedType;
		if (logging || FM_FORCE_LOG) {
			std::cout << "openFileR: expected type matched: " << typeName(expectedType) << '\n';
		}
	}
	else {
		m_fileType = detectedType;
	}

	if (m_fileType == FMFileType::plain) {
		return FMErrorCode::none;
	}

	if (m_fileType == FMFileType::empty) {
		const FMErrorCode result = validateEmptyFormat(logging);
		if (result != FMErrorCode::none) {
			closeFile();
		}
		return result;
	}

	if (m_fileType == FMFileType::naive || m_fileType == FMFileType::huffman) {
		const FMErrorCode result = validateFF(logging);
		if (result != FMErrorCode::none) {
			closeFile();
		}
		return result;
	}

	closeFile();
	return FMErrorCode::internal_error;
}

void FileManager::closeFile() {
	if (!m_file) {
		m_file = std::make_shared<std::fstream>();
		resetState();
		return;
	}

	m_file->clear();
	if (m_file->is_open()) {
		m_file->close();
	}
	resetState();
}

bool FileManager::isFileOpen() const {
	return m_file != nullptr && m_file->is_open();
}

FMFileType FileManager::getFileType() const noexcept {
	return m_fileType;
}

std::uint32_t FileManager::getCRC() const noexcept {
	return m_crc32;
}

std::size_t FileManager::getDictLength() const noexcept {
	return m_dictLength;
}

std::uint64_t FileManager::getEncodedPayloadLength() const noexcept {
	return m_encodedPayloadLength;
}

std::uint64_t FileManager::getDecodedPayloadLength() const noexcept {
	return m_decodedPayloadLength;
}

FMErrorCode FileManager::jumpToData(bool logging) {
	if (!isFileOpen()) {
		return FMErrorCode::file_not_open;
	}

	if (!isDictionaryType(m_fileType)) {
		return FMErrorCode::wrong_type;
	}

	if (m_payloadPos == invalidPosition) {
		return FMErrorCode::invalid_format;
	}

	m_file->clear();
	m_file->seekg(m_payloadPos, std::ios::beg);
	if (!m_file->good()) {
		return FMErrorCode::file_seek_failed;
	}

	if (logging || FM_FORCE_LOG) {
		std::cout << "jumpToData: encoded payload bytes: " << m_encodedPayloadLength << '\n';
		std::cout << "jumpToData: decoded payload bytes: " << m_decodedPayloadLength << '\n';
	}

	return FMErrorCode::none;
}

FMErrorCode FileManager::parseDictionary(
	std::unordered_map<unsigned char, std::string>& outDictionary,
	bool logging
) {
	if (!isFileOpen()) {
		return FMErrorCode::file_not_open;
	}

	if (m_fileType == FMFileType::plain || m_fileType == FMFileType::empty || m_fileType == FMFileType::none) {
		return FMErrorCode::wrong_type;
	}

	if (m_fileType != FMFileType::naive) {
		return FMErrorCode::wrong_type;
	}

	if (m_dictPos == invalidPosition) {
		return FMErrorCode::invalid_format;
	}

	outDictionary.clear();
	std::unordered_set<std::string> seenCodes;
	const std::size_t codeWidth = fmNaiveCodeWidth(m_dictLength);

	m_file->clear();
	m_file->seekg(m_dictPos, std::ios::beg);
	if (!m_file->good()) {
		return FMErrorCode::file_seek_failed;
	}

	if (logging || FM_FORCE_LOG) {
		std::cout << "parseDictionary(naive): dictionary length: " << m_dictLength << '\n';
	}

	for (std::size_t index = 0u; index < m_dictLength; ++index) {
		std::array<unsigned char, 2> entry{};
		const FMErrorCode readResult = readBytes(entry.data(), entry.size());
		if (readResult != FMErrorCode::none) {
			return readResult;
		}

		const unsigned char symbol = entry[0];
		const unsigned char codeByte = entry[1];

		std::string code;
		code.reserve(codeWidth);
		for (std::size_t bitIndex = 0u; bitIndex < codeWidth; ++bitIndex) {
			const std::size_t shift = codeWidth - 1u - bitIndex;
			const bool bit = ((codeByte >> shift) & 0x01u) != 0u;
			code.push_back(bit ? '1' : '0');
		}

		const auto [it, inserted] = outDictionary.emplace(symbol, code);
		if (!inserted) {
			return FMErrorCode::duplicate_dictionary_entry;
		}

		if (!seenCodes.insert(code).second) {
			return FMErrorCode::duplicate_dictionary_code;
		}

		if (logging || FM_FORCE_LOG) {
			std::cout << "parseDictionary(naive): "
				<< hexByte(symbol) << " -> " << code << " (ascii: " << symbol << ") \n";
		}
	}

	return FMErrorCode::none;
}

FMErrorCode FileManager::parseDictionary(HuffNode*& outRoot, bool logging) {
	if (!isFileOpen()) {
		return FMErrorCode::file_not_open;
	}

	if (m_fileType == FMFileType::plain || m_fileType == FMFileType::empty || m_fileType == FMFileType::none) {
		return FMErrorCode::wrong_type;
	}

	if (m_fileType != FMFileType::huffman) {
		return FMErrorCode::wrong_type;
	}

	if (m_dictPos == invalidPosition) {
		return FMErrorCode::invalid_format;
	}

	if (outRoot != nullptr) {
		deleteTree(outRoot);
		outRoot = nullptr;
	}

	std::unordered_set<unsigned char> seenSymbols;
	std::unordered_set<std::string> seenCodes;

	m_file->clear();
	m_file->seekg(m_dictPos, std::ios::beg);
	if (!m_file->good()) {
		return FMErrorCode::file_seek_failed;
	}

	if (logging || FM_FORCE_LOG) {
		std::cout << "parseDictionary(huffman): dictionary length: " << m_dictLength << '\n';
	}

	for (std::size_t index = 0u; index < m_dictLength; ++index) {
		std::array<unsigned char, 2> header{};
		const FMErrorCode readHeader = readBytes(header.data(), header.size());
		if (readHeader != FMErrorCode::none) {
			deleteTree(outRoot);
			outRoot = nullptr;
			return readHeader;
		}

		const unsigned char symbol = header[0];
		const std::size_t bitLength = static_cast<std::size_t>(header[1]) + 1u;
		const std::size_t byteLength = fmBitCountToByteCount(bitLength);
		std::vector<unsigned char> storedBytes(byteLength, 0u);
		const FMErrorCode readCode = readBytes(storedBytes.data(), storedBytes.size());
		if (readCode != FMErrorCode::none) {
			deleteTree(outRoot);
			outRoot = nullptr;
			return readCode;
		}

		if (!hasZeroCodePadding(storedBytes, bitLength)) {
			deleteTree(outRoot);
			outRoot = nullptr;
			return FMErrorCode::invalid_padding_bits;
		}

		const std::string code = unpackDictionaryCode(storedBytes, bitLength);
		if (!seenSymbols.insert(symbol).second) {
			deleteTree(outRoot);
			outRoot = nullptr;
			return FMErrorCode::duplicate_dictionary_entry;
		}

		if (!seenCodes.insert(code).second) {
			deleteTree(outRoot);
			outRoot = nullptr;
			return FMErrorCode::duplicate_dictionary_code;
		}

		const FMErrorCode insertResult = insertHuffmanCode(outRoot, symbol, code);
		if (insertResult != FMErrorCode::none) {
			deleteTree(outRoot);
			outRoot = nullptr;
			return insertResult;
		}

		if (logging || FM_FORCE_LOG) {
			std::cout << "parseDictionary(huffman): "
				<< hexByte(symbol) << " -> " << code << " (ascii: "<<symbol << ") \n";
		}
	}

	return FMErrorCode::none;
}

std::shared_ptr<std::fstream> FileManager::detachStream() {
	std::shared_ptr<std::fstream> detached = m_file;
	m_file = std::make_shared<std::fstream>();
	resetState();
	return detached;
}

FMErrorCode FileManager::writePlain(
	std::string_view fileName,
	const std::vector<unsigned char>& data
) const {
	std::fstream outFile(std::string(fileName), std::ios::binary | std::ios::out | std::ios::trunc);
	if (!outFile.is_open()) {
		return FMErrorCode::file_open_failed;
	}

	const FMErrorCode writeResult = writeBytes(outFile, data.data(), data.size());
	if (writeResult != FMErrorCode::none) {
		return writeResult;
	}

	outFile.flush();
	if (!outFile.good()) {
		return FMErrorCode::file_write_failed;
	}

	return FMErrorCode::none;
}

FMErrorCode FileManager::writeFormat(
	std::string_view fileName,
	const std::vector<unsigned char>& encodedPayload,
	std::uint64_t originalDecodedLength,
	const std::unordered_map<unsigned char, std::string>& dictionary,
	bool logging,
	bool addExtension
) const {
	if (encodedPayload.empty()) {
		if (originalDecodedLength != 0u) {
			return FMErrorCode::invalid_payload_length;
		}
		return writeEmptyFile(fileName, logging);
	}

	if (dictionary.empty()) {
		return FMErrorCode::empty_dictionary;
	}

	if (dictionary.size() > 256u) {
		return FMErrorCode::invalid_dictionary_length;
	}



	const std::string outName((addExtension)? buildOutputFileName(fileName, FMFileType::naive) : fileName);


	std::fstream outFile(
		outName,
		std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc
	);
	if (!outFile.is_open()) {
		return FMErrorCode::file_open_failed;
	}

	const auto magic = makeMagic(FMFileType::naive);
	const FMErrorCode writeMagic = writeBytes(outFile, magic.data(), magic.size());
	if (writeMagic != FMErrorCode::none) {
		return writeMagic;
	}

	const std::array<unsigned char, 4> zeroCRC{ 0u, 0u, 0u, 0u };
	const FMErrorCode writeCRCPlaceholder = writeBytes(outFile, zeroCRC.data(), zeroCRC.size());
	if (writeCRCPlaceholder != FMErrorCode::none) {
		return writeCRCPlaceholder;
	}

	const FMErrorCode writeDict = writeNaiveDictionary(outFile, dictionary, logging);
	if (writeDict != FMErrorCode::none) {
		return writeDict;
	}

	const FMErrorCode writePayload = writePayloadSection(outFile, encodedPayload, originalDecodedLength);
	if (writePayload != FMErrorCode::none) {
		return writePayload;
	}

	std::uint32_t crc = 0u;
	const FMErrorCode finalizeCRC = finalizeWrittenCRC(outFile, crc);
	if (finalizeCRC != FMErrorCode::none) {
		return finalizeCRC;
	}

	if (logging || FM_FORCE_LOG) {
		outFile.clear();
		outFile.seekg(0, std::ios::end);
		const std::streamoff fileSize = outFile.tellg();
		std::cout << "writeFormat(naive): magic bytes: " << fmBytesToHex(magic) << '\n';
		std::cout << "writeFormat(naive): type: " << typeName(FMFileType::naive) << '\n';
		std::cout << "writeFormat(naive): encoded payload bytes: " << encodedPayload.size() << '\n';
		std::cout << "writeFormat(naive): decoded payload bytes: " << originalDecodedLength << '\n';
		std::cout << "writeFormat(naive): file size bytes: " << fileSize << '\n';
		std::cout << "writeFormat(naive): crc32c: " << fmBytesToHex(fmU32ToLE(crc)) << '\n';
		std::cout << "writeFormat(naive): filename: " << outName << '\n';
	}

	return FMErrorCode::none;
}

FMErrorCode FileManager::writeFormat(
	std::string_view fileName,
	const std::vector<unsigned char>& encodedPayload,
	std::uint64_t originalDecodedLength,
	const HuffNode* tree,
	bool logging,
	bool addExtension
) const {
	if (encodedPayload.empty()) {
		if (originalDecodedLength != 0u) {
			return FMErrorCode::invalid_payload_length;
		}
		return writeEmptyFile(fileName, logging);
	}

	if (tree == nullptr) {
		return FMErrorCode::null_tree;
	}

	const std::string outName((addExtension) ? buildOutputFileName(fileName, FMFileType::huffman) : fileName);

	std::fstream outFile(
		outName,
		std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc
	);
	if (!outFile.is_open()) {
		return FMErrorCode::file_open_failed;
	}

	const auto magic = makeMagic(FMFileType::huffman);
	const FMErrorCode writeMagic = writeBytes(outFile, magic.data(), magic.size());
	if (writeMagic != FMErrorCode::none) {
		return writeMagic;
	}

	const std::array<unsigned char, 4> zeroCRC{ 0u, 0u, 0u, 0u };
	const FMErrorCode writeCRCPlaceholder = writeBytes(outFile, zeroCRC.data(), zeroCRC.size());
	if (writeCRCPlaceholder != FMErrorCode::none) {
		return writeCRCPlaceholder;
	}

	const FMErrorCode writeDict = writeHuffmanDictionary(outFile, tree, logging);
	if (writeDict != FMErrorCode::none) {
		return writeDict;
	}

	const FMErrorCode writePayload = writePayloadSection(outFile, encodedPayload, originalDecodedLength);
	if (writePayload != FMErrorCode::none) {
		return writePayload;
	}

	std::uint32_t crc = 0u;
	const FMErrorCode finalizeCRC = finalizeWrittenCRC(outFile, crc);
	if (finalizeCRC != FMErrorCode::none) {
		return finalizeCRC;
	}

	if (logging || FM_FORCE_LOG) {
		outFile.clear();
		outFile.seekg(0, std::ios::end);
		const std::streamoff fileSize = outFile.tellg();
		std::cout << "writeFormat(huffman): magic bytes: " << fmBytesToHex(magic) << '\n';
		std::cout << "writeFormat(huffman): type: " << typeName(FMFileType::huffman) << '\n';
		std::cout << "writeFormat(huffman): encoded payload bytes: " << encodedPayload.size() << '\n';
		std::cout << "writeFormat(huffman): decoded payload bytes: " << originalDecodedLength << '\n';
		std::cout << "writeFormat(huffman): file size bytes: " << fileSize << '\n';
		std::cout << "writeFormat(huffman): crc32c: " << fmBytesToHex(fmU32ToLE(crc)) << '\n';
	}

	return FMErrorCode::none;
}

FMErrorCode FileManager::validateEmptyFormat(bool logging) {
	if (!isFileOpen()) {
		return FMErrorCode::file_not_open;
	}

	m_file->clear();
	m_file->seekg(0, std::ios::beg);
	if (!m_file->good()) {
		return FMErrorCode::file_seek_failed;
	}

	std::array<unsigned char, 5> bytes{};
	const FMErrorCode readResult = readBytes(bytes.data(), bytes.size());
	if (readResult != FMErrorCode::none) {
		return readResult;
	}

	if (bytes[0] != magicFormat[0] || bytes[1] != magicFormat[1] || bytes[2] != magicFormat[2]) {
		return FMErrorCode::wrong_magic;
	}

	if (bytes[3] != magicEmpty) {
		return FMErrorCode::wrong_type;
	}

	if (bytes[4] != 0u) {
		return FMErrorCode::wrong_emtpy_format;
	}

	unsigned char extra = 0u;
	m_file->read(reinterpret_cast<char*>(&extra), 1);
	if (m_file->gcount() != 0) {
		m_file->clear();
		return FMErrorCode::unexpected_payload_at_eof;
	}
	m_file->clear();

	m_crc32 = 0u;
	m_dictPos = invalidPosition;
	m_dictLength = 0u;
	m_payloadPos = invalidPosition;
	m_encodedPayloadLength = 0u;
	m_decodedPayloadLength = 0u;

	if (logging || FM_FORCE_LOG) {
		std::cout << "validateEmptyFormat: magic bytes: " << fmBytesToHex(bytes) << '\n';
		std::cout << "validateEmptyFormat: type: " << typeName(FMFileType::empty) << '\n';
	}

	return FMErrorCode::none;
}

FMErrorCode FileManager::validateFF(bool logging) {
	if (!isFileOpen()) {
		return FMErrorCode::file_not_open;
	}

	if (!isDictionaryType(m_fileType)) {
		return FMErrorCode::wrong_type;
	}

	m_file->clear();
	m_file->seekg(0, std::ios::beg);
	if (!m_file->good()) {
		return FMErrorCode::file_seek_failed;
	}

	std::array<unsigned char, 4> magic{};
	const FMErrorCode readMagic = readBytes(magic.data(), magic.size());
	if (readMagic != FMErrorCode::none) {
		return readMagic;
	}

	const auto expectedMagic = makeMagic(m_fileType);
	const bool familyMatches = std::equal(
		magic.begin(),
		magic.begin() + static_cast<std::ptrdiff_t>(magicFormat.size()),
		magicFormat.begin()
	);
	if (!familyMatches) {
		return FMErrorCode::wrong_magic;
	}

	if (magic != expectedMagic) {
		return FMErrorCode::wrong_type;
	}

	std::array<unsigned char, 4> storedCRCBytes{};
	const FMErrorCode readCRC = readBytes(storedCRCBytes.data(), storedCRCBytes.size());
	if (readCRC != FMErrorCode::none) {
		return readCRC;
	}
	const std::uint32_t storedCRC = fmLEToU32(storedCRCBytes);

	const FMErrorCode calcResult = calcCRC32();
	if (calcResult != FMErrorCode::none) {
		return calcResult;
	}
	const std::uint32_t calculatedCRC = m_crc32;
	m_crc32 = storedCRC;

	if (logging || FM_FORCE_LOG) {
		std::cout << "validateFF: stored crc32c: " << fmBytesToHex(storedCRCBytes) << '\n';
		std::cout << "validateFF: calculated crc32c: " << fmBytesToHex(fmU32ToLE(calculatedCRC)) << '\n';
	}

	if (storedCRC != calculatedCRC) {
		return FMErrorCode::crc32_mismatch;
	}

	m_file->clear();
	m_file->seekg(static_cast<std::streamoff>(4 + 4), std::ios::beg);
	if (!m_file->good()) {
		return FMErrorCode::file_seek_failed;
	}

	unsigned char storedDictLength = 0u;
	const FMErrorCode readDictLength = readBytes(&storedDictLength, 1u);
	if (readDictLength != FMErrorCode::none) {
		return readDictLength;
	}

	m_dictLength = static_cast<std::size_t>(storedDictLength) + 1u;
	m_dictPos = m_file->tellg();
	if (m_dictPos == invalidPosition) {
		return FMErrorCode::file_seek_failed;
	}

	if (m_fileType == FMFileType::naive) {
		const std::size_t codeWidth = fmNaiveCodeWidth(m_dictLength);
		if (codeWidth > 8u) {
			return FMErrorCode::invalid_dictionary_code;
		}

		for (std::size_t index = 0u; index < m_dictLength; ++index) {
			std::array<unsigned char, 2> entry{};
			const FMErrorCode readEntry = readBytes(entry.data(), entry.size());
			if (readEntry != FMErrorCode::none) {
				return readEntry;
			}

			if (codeWidth < 8u) {
				const unsigned char mask = static_cast<unsigned char>(0xFFu << codeWidth);
				if ((entry[1] & mask) != 0u) {
					return FMErrorCode::invalid_padding_bits;
				}
			}
		}
	}
	else {
		for (std::size_t index = 0u; index < m_dictLength; ++index) {
			std::array<unsigned char, 2> entryHeader{};
			const FMErrorCode readEntryHeader = readBytes(entryHeader.data(), entryHeader.size());
			if (readEntryHeader != FMErrorCode::none) {
				return readEntryHeader;
			}

			const std::size_t bitLength = static_cast<std::size_t>(entryHeader[1]) + 1u;
			const std::size_t codeByteCount = fmBitCountToByteCount(bitLength);
			if (codeByteCount == 0u || codeByteCount > 32u) {
				return FMErrorCode::invalid_dictionary_code;
			}

			std::vector<unsigned char> codeBytes(codeByteCount, 0u);
			const FMErrorCode readCodeBytes = readBytes(codeBytes.data(), codeBytes.size());
			if (readCodeBytes != FMErrorCode::none) {
				return readCodeBytes;
			}

			if (!hasZeroCodePadding(codeBytes, bitLength)) {
				return FMErrorCode::invalid_padding_bits;
			}
		}
	}

	std::array<unsigned char, 8> encodedLenBytes{};
	const FMErrorCode readEncodedLen = readBytes(encodedLenBytes.data(), encodedLenBytes.size());
	if (readEncodedLen != FMErrorCode::none) {
		return readEncodedLen;
	}
	m_encodedPayloadLength = fmLEToU64(encodedLenBytes);

	std::array<unsigned char, 8> decodedLenBytes{};
	const FMErrorCode readDecodedLen = readBytes(decodedLenBytes.data(), decodedLenBytes.size());
	if (readDecodedLen != FMErrorCode::none) {
		return readDecodedLen;
	}
	m_decodedPayloadLength = fmLEToU64(decodedLenBytes);

	m_payloadPos = m_file->tellg();
	if (m_payloadPos == invalidPosition) {
		return FMErrorCode::file_seek_failed;
	}

	if (m_fileType == FMFileType::naive) {
		const std::size_t codeWidth = fmNaiveCodeWidth(m_dictLength);
		const std::uint64_t expectedBytes = expectedNaivePayloadBytes(m_decodedPayloadLength, codeWidth);
		if (expectedBytes != m_encodedPayloadLength) {
			return FMErrorCode::invalid_payload_length;
		}
	}

	m_file->clear();
	m_file->seekg(0, std::ios::end);
	if (!m_file->good()) {
		return FMErrorCode::file_seek_failed;
	}

	const std::streamoff fileEnd = m_file->tellg();
	if (fileEnd == invalidPosition || m_payloadPos < 0) {
		return FMErrorCode::file_seek_failed;
	}

	const std::uint64_t payloadPos = static_cast<std::uint64_t>(m_payloadPos);
	if (std::numeric_limits<std::uint64_t>::max() - payloadPos < m_encodedPayloadLength) {
		return FMErrorCode::invalid_payload_length;
	}

	const std::uint64_t expectedEnd = payloadPos + m_encodedPayloadLength;
	const std::uint64_t actualEnd = static_cast<std::uint64_t>(fileEnd);
	if (actualEnd < expectedEnd) {
		return FMErrorCode::unexpected_eof;
	}
	if (actualEnd > expectedEnd) {
		return FMErrorCode::unexpected_payload_at_eof;
	}

	m_file->clear();
	m_file->seekg(0, std::ios::beg);
	if (!m_file->good()) {
		return FMErrorCode::file_seek_failed;
	}

	if (logging || FM_FORCE_LOG) {
		std::cout << "validateFF: type: " << typeName(m_fileType) << '\n';
		std::cout << "validateFF: dictionary length: " << m_dictLength << '\n';
		std::cout << "validateFF: encoded payload bytes: " << m_encodedPayloadLength << '\n';
		std::cout << "validateFF: decoded payload bytes: " << m_decodedPayloadLength << '\n';
	}

	return FMErrorCode::none;
}

FMErrorCode FileManager::calcCRC32() {
	if (!isFileOpen()) {
		return FMErrorCode::file_not_open;
	}

	std::uint32_t crc = 0u;
	const FMErrorCode result = calcCRC32(*m_file, crc);
	if (result != FMErrorCode::none) {
		return result;
	}

	m_crc32 = crc;
	return FMErrorCode::none;
}

FMErrorCode FileManager::calcCRC32(std::fstream& file, std::uint32_t& outCRC) {
	if (!file.is_open()) {
		return FMErrorCode::file_not_open;
	}

	const std::streamoff startPos = file.tellg();
	if (startPos == invalidPosition) {
		return FMErrorCode::file_seek_failed;
	}

	file.clear();
	std::uint32_t crc = 0xFFFFFFFFu;
	std::array<unsigned char, 4096> buffer{};
	while (true) {
		file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
		const std::streamsize got = file.gcount();
		if (got < 0) {
			file.clear();
			file.seekg(startPos, std::ios::beg);
			return FMErrorCode::file_read_failed;
		}

		for (std::streamsize index = 0; index < got; ++index) {
			crc ^= static_cast<std::uint32_t>(buffer[static_cast<std::size_t>(index)]);
			for (int bit = 0; bit < 8; ++bit) {
				const bool lowBit = (crc & 1u) != 0u;
				crc >>= 1u;
				if (lowBit) {
					crc ^= crc32cPolynomial;
				}
			}
		}

		if (file.bad()) {
			file.clear();
			file.seekg(startPos, std::ios::beg);
			return FMErrorCode::file_read_failed;
		}

		if (file.eof()) {
			break;
		}
	}

	file.clear();
	file.seekg(startPos, std::ios::beg);
	if (!file.good()) {
		return FMErrorCode::file_seek_failed;
	}

	outCRC = crc ^ 0xFFFFFFFFu;
	return FMErrorCode::none;
}

FMErrorCode FileManager::readBytes(unsigned char* buffer, std::size_t count) {
	if (!isFileOpen()) {
		return FMErrorCode::file_not_open;
	}

	if (count == 0u) {
		return FMErrorCode::none;
	}

	m_file->read(reinterpret_cast<char*>(buffer), static_cast<std::streamsize>(count));
	if (m_file->good()) {
		return FMErrorCode::none;
	}

	if (m_file->eof() && static_cast<std::size_t>(m_file->gcount()) != count) {
		m_file->clear();
		return FMErrorCode::unexpected_eof;
	}

	if (m_file->bad() || m_file->fail()) {
		m_file->clear();
		return FMErrorCode::file_read_failed;
	}

	return FMErrorCode::none;
}

FMErrorCode FileManager::writeBytes(
	std::fstream& outFile,
	const unsigned char* buffer,
	std::size_t count
) {
	if (count == 0u) {
		return FMErrorCode::none;
	}

	outFile.write(reinterpret_cast<const char*>(buffer), static_cast<std::streamsize>(count));
	if (!outFile.good()) {
		return FMErrorCode::file_write_failed;
	}

	return FMErrorCode::none;
}

FMFileType FileManager::detectTypeFromMagic(
	const std::array<unsigned char, 4>& magic
) noexcept {
	if (!std::equal(magic.begin(), magic.begin() + static_cast<std::ptrdiff_t>(magicFormat.size()), magicFormat.begin())) {
		return FMFileType::plain;
	}

	if (magic[3] == magicNaive) {
		return FMFileType::naive;
	}
	if (magic[3] == magicHuffman) {
		return FMFileType::huffman;
	}
	if (magic[3] == magicEmpty) {
		return FMFileType::empty;
	}

	return FMFileType::plain;
}

std::array<unsigned char, 4> FileManager::makeMagic(FMFileType type) noexcept {
	switch (type) {
		case FMFileType::naive:
			return { magicFormat[0], magicFormat[1], magicFormat[2], magicNaive };
		case FMFileType::huffman:
			return { magicFormat[0], magicFormat[1], magicFormat[2], magicHuffman };
		case FMFileType::empty:
			return { magicFormat[0], magicFormat[1], magicFormat[2], magicEmpty };
		default:
			return { 0u, 0u, 0u, 0u };
	}
}

bool FileManager::isWrappedType(FMFileType type) noexcept {
	return type == FMFileType::empty || type == FMFileType::naive || type == FMFileType::huffman;
}

bool FileManager::isDictionaryType(FMFileType type) noexcept {
	return type == FMFileType::naive || type == FMFileType::huffman;
}

const char* FileManager::typeName(FMFileType type) noexcept {
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

std::string FileManager::buildOutputFileName(
	std::string_view fileName,
	FMFileType type
) {
	std::string outName(fileName);
	std::string_view extension;
	switch (type) {
		case FMFileType::naive:
			extension = extNaive;
			break;
		case FMFileType::huffman:
			extension = extHuffman;
			break;
		case FMFileType::empty:
			extension = extEmpty;
			break;
		default:
			return outName;
	}

	if (outName.size() >= extension.size()
		&& outName.compare(outName.size() - extension.size(), extension.size(), extension) == 0) {
		return outName;
	}

	outName += extension;
	return outName;
}

FMErrorCode FileManager::writeEmptyFile(std::string_view fileName, bool logging) {
	const std::string outName = buildOutputFileName(fileName, FMFileType::empty);
	std::fstream outFile(outName, std::ios::binary | std::ios::out | std::ios::trunc);
	if (!outFile.is_open()) {
		return FMErrorCode::file_open_failed;
	}

	const auto magic = makeMagic(FMFileType::empty);
	const FMErrorCode writeMagic = writeBytes(outFile, magic.data(), magic.size());
	if (writeMagic != FMErrorCode::none) {
		return writeMagic;
	}

	const unsigned char trailingZero = 0u;
	const FMErrorCode writeZero = writeBytes(outFile, &trailingZero, 1u);
	if (writeZero != FMErrorCode::none) {
		return writeZero;
	}

	outFile.flush();
	if (!outFile.good()) {
		return FMErrorCode::file_write_failed;
	}

	if (logging || FM_FORCE_LOG) {
		const std::array<unsigned char, 5> bytes{ magic[0], magic[1], magic[2], magic[3], trailingZero };
		std::cout << "writeEmptyFile: magic bytes: " << fmBytesToHex(bytes) << '\n';
		std::cout << "writeEmptyFile: type: " << typeName(FMFileType::empty) << '\n';
		std::cout << "writeEmptyFile: file size bytes: 5\n";
	}

	return FMErrorCode::none;
}

FMErrorCode FileManager::writeNaiveDictionary(
	std::fstream& outFile,
	const std::unordered_map<unsigned char, std::string>& dictionary,
	bool logging
) {
	if (dictionary.empty()) {
		return FMErrorCode::empty_dictionary;
	}

	if (dictionary.size() > 256u) {
		return FMErrorCode::invalid_dictionary_length;
	}

	const std::size_t codeWidth = fmNaiveCodeWidth(dictionary.size());
	std::vector<std::pair<unsigned char, std::string>> entries(dictionary.begin(), dictionary.end());
	std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
		return left.first < right.first;
		});

	std::unordered_set<std::string> seenCodes;
	const unsigned char storedLength = static_cast<unsigned char>(entries.size() - 1u);
	const FMErrorCode writeLen = writeBytes(outFile, &storedLength, 1u);
	if (writeLen != FMErrorCode::none) {
		return writeLen;
	}

	if (logging || FM_FORCE_LOG) {
		std::cout << "writeNaiveDictionary: dictionary length: " << entries.size() << '\n';
	}

	for (const auto& [symbol, code] : entries) {
		if (code.size() != codeWidth) {
			return FMErrorCode::invalid_dictionary_code;
		}
		if (!std::all_of(code.begin(), code.end(), [](char ch) { return ch == '0' || ch == '1'; })) {
			return FMErrorCode::invalid_dictionary_code;
		}
		if (!seenCodes.insert(code).second) {
			return FMErrorCode::duplicate_dictionary_code;
		}

		const std::vector<unsigned char> packed = packDictionaryCode(code);
		if (packed.size() != 1u || !hasZeroCodePadding(packed, code.size())) {
			return FMErrorCode::invalid_dictionary_code;
		}

		const std::array<unsigned char, 2> entry{ symbol, packed[0] };
		const FMErrorCode writeEntry = writeBytes(outFile, entry.data(), entry.size());
		if (writeEntry != FMErrorCode::none) {
			return writeEntry;
		}

		if (logging || FM_FORCE_LOG) {
			std::cout << "writeNaiveDictionary: "
				<< hexByte(symbol) << " -> " << code << " (ascii: " << symbol << ") \n";
		}
	}

	return FMErrorCode::none;
}

FMErrorCode FileManager::writeHuffmanDictionary(
	std::fstream& outFile,
	const HuffNode* tree,
	bool logging
) {
	if (tree == nullptr) {
		return FMErrorCode::null_tree;
	}

	std::vector<std::pair<unsigned char, std::string>> codes;
	collectHuffmanCodes(tree, std::string{}, codes);
	if (codes.empty()) {
		return FMErrorCode::empty_dictionary;
	}
	if (codes.size() > 256u) {
		return FMErrorCode::invalid_dictionary_length;
	}

	std::sort(codes.begin(), codes.end(), [](const auto& left, const auto& right) {
		return left.first < right.first;
		});

	std::unordered_set<unsigned char> seenSymbols;
	std::unordered_set<std::string> seenCodes;
	const unsigned char storedLength = static_cast<unsigned char>(codes.size() - 1u);
	const FMErrorCode writeLen = writeBytes(outFile, &storedLength, 1u);
	if (writeLen != FMErrorCode::none) {
		return writeLen;
	}

	if (logging || FM_FORCE_LOG) {
		std::cout << "writeHuffmanDictionary: dictionary length: " << codes.size() << '\n';
	}

	for (const auto& [symbol, code] : codes) {
		if (code.empty() || code.size() > 256u) {
			return FMErrorCode::invalid_dictionary_code;
		}
		if (!std::all_of(code.begin(), code.end(), [](char ch) { return ch == '0' || ch == '1'; })) {
			return FMErrorCode::invalid_dictionary_code;
		}
		if (!seenSymbols.insert(symbol).second) {
			return FMErrorCode::duplicate_dictionary_entry;
		}
		if (!seenCodes.insert(code).second) {
			return FMErrorCode::duplicate_dictionary_code;
		}

		const std::vector<unsigned char> packed = packDictionaryCode(code);
		if (!hasZeroCodePadding(packed, code.size())) {
			return FMErrorCode::invalid_padding_bits;
		}

		const unsigned char symbolByte = symbol;
		const unsigned char lengthByte = static_cast<unsigned char>(code.size() - 1u);
		const FMErrorCode writeSymbol = writeBytes(outFile, &symbolByte, 1u);
		if (writeSymbol != FMErrorCode::none) {
			return writeSymbol;
		}
		const FMErrorCode writeLength = writeBytes(outFile, &lengthByte, 1u);
		if (writeLength != FMErrorCode::none) {
			return writeLength;
		}
		const FMErrorCode writeCode = writeBytes(outFile, packed.data(), packed.size());
		if (writeCode != FMErrorCode::none) {
			return writeCode;
		}

		if (logging || FM_FORCE_LOG) {
			std::cout << "writeHuffmanDictionary: "
				<< hexByte(symbol) << " -> " << code << " (ascii: " << symbol << ") \n";
		}
	}

	return FMErrorCode::none;
}

FMErrorCode FileManager::writePayloadSection(
	std::fstream& outFile,
	const std::vector<unsigned char>& encodedPayload,
	std::uint64_t originalDecodedLength
) {
	const auto encodedLenBytes = fmU64ToLE(static_cast<std::uint64_t>(encodedPayload.size()));
	const auto decodedLenBytes = fmU64ToLE(originalDecodedLength);

	const FMErrorCode writeEncodedLen = writeBytes(outFile, encodedLenBytes.data(), encodedLenBytes.size());
	if (writeEncodedLen != FMErrorCode::none) {
		return writeEncodedLen;
	}

	const FMErrorCode writeDecodedLen = writeBytes(outFile, decodedLenBytes.data(), decodedLenBytes.size());
	if (writeDecodedLen != FMErrorCode::none) {
		return writeDecodedLen;
	}

	return writeBytes(outFile, encodedPayload.data(), encodedPayload.size());
}

FMErrorCode FileManager::finalizeWrittenCRC(std::fstream& outFile, std::uint32_t& outCRC) {
	outFile.flush();
	if (!outFile.good()) {
		return FMErrorCode::file_write_failed;
	}

	outFile.clear();
	outFile.seekg(static_cast<std::streamoff>(4 + 4), std::ios::beg);
	if (!outFile.good()) {
		return FMErrorCode::file_seek_failed;
	}

	const FMErrorCode calcResult = calcCRC32(outFile, outCRC);
	if (calcResult != FMErrorCode::none) {
		return calcResult;
	}

	const auto crcBytes = fmU32ToLE(outCRC);
	outFile.clear();
	outFile.seekp(4, std::ios::beg);
	if (!outFile.good()) {
		return FMErrorCode::file_seek_failed;
	}

	const FMErrorCode writeCRC = writeBytes(outFile, crcBytes.data(), crcBytes.size());
	if (writeCRC != FMErrorCode::none) {
		return writeCRC;
	}

	outFile.flush();
	if (!outFile.good()) {
		return FMErrorCode::file_write_failed;
	}

	return FMErrorCode::none;
}

void FileManager::collectHuffmanCodes(
	const HuffNode* node,
	const std::string& prefix,
	std::vector<std::pair<unsigned char, std::string>>& outCodes
) {
	if (node == nullptr) {
		return;
	}

	if (node->left == nullptr && node->right == nullptr) {
		outCodes.emplace_back(node->data, prefix.empty() ? std::string("0") : prefix);
		return;
	}

	collectHuffmanCodes(node->left, prefix + '0', outCodes);
	collectHuffmanCodes(node->right, prefix + '1', outCodes);
}

std::vector<unsigned char> FileManager::packDictionaryCode(std::string_view code) {
	const std::size_t bitLength = code.size();
	std::vector<unsigned char> storedBytes(fmBitCountToByteCount(bitLength), 0u);
	for (std::size_t index = 0u; index < bitLength; ++index) {
		if (code[index] != '1') {
			continue;
		}

		const std::size_t numericBit = bitLength - 1u - index;
		const std::size_t byteIndex = numericBit / 8u;
		const std::size_t bitIndex = numericBit % 8u;
		storedBytes[byteIndex] = static_cast<unsigned char>(
			storedBytes[byteIndex] | static_cast<unsigned char>(1u << bitIndex)
			);
	}
	return storedBytes;
}

std::string FileManager::unpackDictionaryCode(
	const std::vector<unsigned char>& storedBytes,
	std::size_t bitLength
) {
	std::string code;
	code.reserve(bitLength);
	for (std::size_t index = 0u; index < bitLength; ++index) {
		const std::size_t numericBit = bitLength - 1u - index;
		const std::size_t byteIndex = numericBit / 8u;
		const std::size_t bitIndex = numericBit % 8u;
		const bool bit = ((storedBytes[byteIndex] >> bitIndex) & 0x01u) != 0u;
		code.push_back(bit ? '1' : '0');
	}
	return code;
}

bool FileManager::hasZeroCodePadding(
	const std::vector<unsigned char>& storedBytes,
	std::size_t bitLength
) {
	if (bitLength == 0u) {
		return false;
	}

	const std::size_t expectedBytes = fmBitCountToByteCount(bitLength);
	if (storedBytes.size() != expectedBytes) {
		return false;
	}

	const std::size_t usedBitsInFinalByte = bitLength % 8u;
	if (usedBitsInFinalByte == 0u) {
		return true;
	}

	const unsigned char finalByte = storedBytes.back();
	const unsigned char mask = static_cast<unsigned char>(0xFFu << usedBitsInFinalByte);
	return (finalByte & mask) == 0u;
}

FMErrorCode FileManager::insertHuffmanCode(
	HuffNode*& root,
	unsigned char symbol,
	std::string_view code
) {
	if (code.empty()) {
		return FMErrorCode::invalid_dictionary_code;
	}

	if (root == nullptr) {
		root = new HuffNode(0, nullptr, nullptr);
	}

	HuffNode* node = root;
	for (std::size_t index = 0u; index < code.size(); ++index) {
		const char bit = code[index];
		if (bit != '0' && bit != '1') {
			return FMErrorCode::invalid_dictionary_code;
		}

		HuffNode*& next = (bit == '0') ? node->left : node->right;
		const bool isLast = (index + 1u == code.size());
		if (isLast) {
			if (next != nullptr) {
				if (isLeaf(next)) {
					return FMErrorCode::duplicate_dictionary_code;
				}
				return FMErrorCode::invalid_dictionary_code;
			}

			next = new HuffNode(symbol, 0);
			return FMErrorCode::none;
		}

		if (next == nullptr) {
			next = new HuffNode(0, nullptr, nullptr);
		}
		else if (isLeaf(next)) {
			return FMErrorCode::invalid_dictionary_code;
		}

		node = next;
	}

	return FMErrorCode::internal_error;
}
