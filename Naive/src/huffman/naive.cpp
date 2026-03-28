#include "huffman/naive.hpp"
#include <cmath>
#include <unordered_set>
#include <istream>

std::unordered_map<std::byte, std::string> createHuffmanMap(std::istream& file) {
    std::unordered_set<std::byte> bytes;
    char c;
    while (file.get(c)) {
        bytes.insert(static_cast<std::byte>(static_cast<unsigned char>(c)));
    }
    int total = (bytes.size() <= 1) ? 1 : (int)std::ceil(std::log2(bytes.size()));

    std::unordered_map<std::byte, std::string> map;
    int code = 0;
    for (std::byte currentByte : bytes) {
        std::string bits = "";
        for (int i = total - 1; i >= 0; --i) {
            if ((code & (1 << i)) != 0) {
                bits += '1';
            } else {
                bits += '0';
            }
        }
        map[currentByte] = bits;
        ++code;
    }
    file.clear();
    file.seekg(0);
    return map;
}


std::vector<std::byte> encode(std::istream& file, const std::unordered_map<std::byte, std::string> map) {
    std::vector<std::byte> encodedData;
    std::byte buffer{0};
    int bitPos = 7;

    char c;
    while (file.get(c)) {

        const std::string& bits = map.at(std::byte(static_cast<unsigned char>(c)));

        for (char bit : bits) {
            if (bit == '1') {
                buffer |= std::byte(1 << bitPos);
            }
            --bitPos;
            if (bitPos < 0) {
                encodedData.push_back(buffer);
                buffer = std::byte{0};
                bitPos = 7;
            }
        }
    }
    if (bitPos != 7) {
        encodedData.push_back(buffer);
    }

    file.clear();
    file.seekg(0);
    return encodedData;

}

std::vector<std::byte> decode(std::istream& file, const std::unordered_map<std::byte, std::string> map) {

    std::unordered_map<std::string, std::byte> reverseMap;
    for (const auto& [byte, bits] : map) {
        reverseMap[bits] = byte;
    }

    std::vector<std::byte> decodedData;
    std::string current;
    char c;
    int total = map.empty() ? 0 : map.begin()->second.size();
    current.reserve(total);
    while (file.get(c)) {
        std::byte encodedByte = static_cast<std::byte>(static_cast<unsigned char>(c));

        for (int i = 7; i >= 0; --i) {
            if ((encodedByte & std::byte(1 << i)) != std::byte(0)) {
                current += '1';
            } else {
                current += '0';
            }

            if ((int)current.size() == total) {
                auto it = reverseMap.find(current);
                if (it != reverseMap.end()) {
                    decodedData.push_back(it->second);
                }
                current.clear();
            }
        }
    }

    file.clear();
    file.seekg(0);
    return decodedData;
}
