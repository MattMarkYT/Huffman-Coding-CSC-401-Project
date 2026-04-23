#ifndef FILE_MANAGER_HPP
#define FILE_MANAGER_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "huffman/FMMisc.hpp"
#include "huffman/HuffNode.hpp"

#define FM_FORCE_LOG true


// These are the status codes returned by FileManager operations.
enum class FMErrorCode {
	none = 0,
	file_not_open,
	file_open_failed,
	bad_stream_state,
	file_read_failed,
	file_write_failed,
	file_seek_failed,
	wrong_magic,
	wrong_type,
	crc32_mismatch,
	wrong_emtpy_format,
	unexpected_payload_at_eof,
	unexpected_eof,
	invalid_format,
	invalid_dictionary_length,
	invalid_dictionary_entry,
	invalid_dictionary_code,
	invalid_padding_bits,
	invalid_payload_length,
	duplicate_dictionary_entry,
	duplicate_dictionary_code,
	empty_dictionary,
	null_tree,
	internal_error
};

// These are the file kinds that the manager can detect or enforce.
enum class FMFileType {
	none = 0,
	plain,
	empty,
	naive,
	huffman
};

class FileManager {
public:
	
	// These are the format-family bytes that all wrapped files start with.
	static constexpr std::array<unsigned char, 3> magicFormat{ 0xC4u, 0x01u, 0xFEu };

	// These are the type designators for wrapped files.
	static constexpr unsigned char magicNaive = 0xAAu;
	static constexpr unsigned char magicHuffman = 0xCEu;
	static constexpr unsigned char magicEmpty = 0xEFu;

	// This is the reversed Castagnoli polynomial for the usual reflected CRC32C loop.
	static constexpr std::uint32_t crc32cPolynomial = 0x82F63B78u;

	FileManager() = default;
	~FileManager() = default;

	// This opens for reading, detects or checks the wrapper type,
	// skips wrapper validation for plain files, and validates wrapped files automatically for empty, naive, or huffman.
	// Successful wrapped opens leave cached metadata populated for later parse and seek steps.
	[[nodiscard]] FMErrorCode openFileR(
		std::string_view fileName,
		FMFileType expectedType = FMFileType::none,
		bool logging = false
	);

	// This closes the current file and clears cached state.
	void closeFile();

	// This reports whether the current shared fstream is open.
	[[nodiscard]] bool isFileOpen() const;

	// These just return cached metadata.
	[[nodiscard]] FMFileType getFileType() const noexcept;
	[[nodiscard]] std::uint32_t getCRC() const noexcept;
	[[nodiscard]] std::size_t getDictLength() const noexcept;
	[[nodiscard]] std::uint64_t getEncodedPayloadLength() const noexcept;
	[[nodiscard]] std::uint64_t getDecodedPayloadLength() const noexcept;

	// This seeks straight to the encoded payload using cached metadata only.
	// It does not do another parsing pass.
	[[nodiscard]] FMErrorCode jumpToData(bool logging = false);

	// This parses the cached naive dictionary semantically into the exact uploaded map type.
	[[nodiscard]] FMErrorCode parseDictionary(
		std::unordered_map<unsigned char, std::string>& outDictionary,
		bool logging = false
	);

	// This parses the cached Huffman dictionary semantically into the exact uploaded tree type.
	[[nodiscard]] FMErrorCode parseDictionary(HuffNode*& outRoot, bool logging = false);

	// This hands the current stream to the caller and resets this instance.
	[[nodiscard]] std::shared_ptr<std::fstream> detachStream();

	// This writes plain bytes with no custom wrapper and does not touch internal state.
	[[nodiscard]] FMErrorCode writePlain(
		std::string_view fileName,
		const std::vector<unsigned char>& data
	) const;

	// This writes the naive wrapper around an already-encoded payload and dictionary.
	[[nodiscard]] FMErrorCode writeFormat(
		std::string_view fileName,
		const std::vector<unsigned char>& encodedPayload,
		std::uint64_t originalDecodedLength,
		const std::unordered_map<unsigned char, std::string>& dictionary,
		bool logging = false,
		bool addExtension = true
	) const;

	// This writes the Huffman wrapper around an already-encoded payload and tree.
	[[nodiscard]] FMErrorCode writeFormat(
		std::string_view fileName,
		const std::vector<unsigned char>& encodedPayload,
		std::uint64_t originalDecodedLength,
		const HuffNode* tree,
		bool logging = false,
		bool addExtension = true
	) const;

	[[nodiscard]] static std::string buildOutputFileName(
		std::string_view fileName,
		FMFileType type
	);
private:
	

	// These are the fixed extensions used when writeFormat picks the output name.
	static constexpr std::string_view extNaive = ".hnai";
	static constexpr std::string_view extHuffman = ".hhuf";
	static constexpr std::string_view extEmpty = ".hemt";

	// This is a sentinel for positions that have not been populated.
	static constexpr std::streamoff invalidPosition = static_cast<std::streamoff>(-1);

	// This clears every cached field back to the neutral state.
	void resetState();

	// This validates the currently opened special empty wrapper.
	[[nodiscard]] FMErrorCode validateEmptyFormat(bool logging = false);

	// This validates the currently opened naive or huffman wrapper structurally and fills cached metadata.
	[[nodiscard]] FMErrorCode validateFF(bool logging = false);

	// This calculates CRC32C from the current internal stream cursor to EOF and updates the cached CRC field.
	[[nodiscard]] FMErrorCode calcCRC32();

	// This calculates CRC32C from the current cursor of an arbitrary file stream.
	[[nodiscard]] static FMErrorCode calcCRC32(std::fstream& file, std::uint32_t& outCRC);

	// This reads an exact byte count from the currently attached stream.
	[[nodiscard]] FMErrorCode readBytes(unsigned char* buffer, std::size_t count);

	// This writes an exact byte count to an output stream.
	[[nodiscard]] static FMErrorCode writeBytes(
		std::fstream& outFile,
		const unsigned char* buffer,
		std::size_t count
	);

	// These are tiny wrapped-type helpers.
	[[nodiscard]] static FMFileType detectTypeFromMagic(
		const std::array<unsigned char, 4>& magic
	) noexcept;
	[[nodiscard]] static std::array<unsigned char, 4> makeMagic(FMFileType type) noexcept;
	[[nodiscard]] static bool isWrappedType(FMFileType type) noexcept;
	[[nodiscard]] static bool isDictionaryType(FMFileType type) noexcept;
	[[nodiscard]] static const char* typeName(FMFileType type) noexcept;

	// This writes the special 5-byte empty wrapper.
	[[nodiscard]] static FMErrorCode writeEmptyFile(
		std::string_view fileName,
		bool logging = false
	);

	// These write only the dictionary sections.
	[[nodiscard]] static FMErrorCode writeNaiveDictionary(
		std::fstream& outFile,
		const std::unordered_map<unsigned char, std::string>& dictionary,
		bool logging = false
	);
	[[nodiscard]] static FMErrorCode writeHuffmanDictionary(
		std::fstream& outFile,
		const HuffNode* tree,
		bool logging = false
	);

	// This writes the 2 length fields and the encoded payload bytes.
	[[nodiscard]] static FMErrorCode writePayloadSection(
		std::fstream& outFile,
		const std::vector<unsigned char>& encodedPayload,
		std::uint64_t originalDecodedLength
	);

	// This recalculates CRC32C after writing and patches it into the wrapped file.
	[[nodiscard]] static FMErrorCode finalizeWrittenCRC(std::fstream& outFile, std::uint32_t& outCRC);

	// This walks the uploaded Huffman tree and extracts the logical code strings.
	static void collectHuffmanCodes(
		const HuffNode* node,
		const std::string& prefix,
		std::vector<std::pair<unsigned char, std::string>>& outCodes
	);

	// This packs a logical code string into the compact on-disk dictionary form.
	// The code is treated as a binary integer and stored in little-endian byte chunks.
	[[nodiscard]] static std::vector<unsigned char> packDictionaryCode(std::string_view code);

	// This unpacks the compact on-disk dictionary form back into a logical code string.
	[[nodiscard]] static std::string unpackDictionaryCode(
		const std::vector<unsigned char>& storedBytes,
		std::size_t bitLength
	);

	// This checks that unused higher bits in the final stored code byte are zero.
	[[nodiscard]] static bool hasZeroCodePadding(
		const std::vector<unsigned char>& storedBytes,
		std::size_t bitLength
	);

	// This inserts one code into a caller-provided HuffNode tree while checking semantic collisions.
	[[nodiscard]] static FMErrorCode insertHuffmanCode(
		HuffNode*& root,
		unsigned char symbol,
		std::string_view code
	);

	// This is the current shared file stream owned by the manager.
	std::shared_ptr<std::fstream> m_file{std::make_shared<std::fstream>()};

	// This is the current file name attached to the manager.
	std::string m_fileName{};

	// This is the detected or assigned file kind.
	FMFileType m_fileType{FMFileType::none};

	// This is the cached CRC32C value.
	std::uint32_t m_crc32{0u};

	// This is the first byte of the first dictionary entry.
	std::streamoff m_dictPos{invalidPosition};

	// This is the dictionary entry count.
	std::size_t m_dictLength{0u};

	// This is the first byte of the encoded payload.
	std::streamoff m_payloadPos{invalidPosition};

	// This is the encoded payload size in bytes.
	std::uint64_t m_encodedPayloadLength{0u};

	// This is the original decoded payload size in bytes.
	std::uint64_t m_decodedPayloadLength{0u};
};

#endif
