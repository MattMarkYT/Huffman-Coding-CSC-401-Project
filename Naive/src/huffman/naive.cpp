#include "huffman/naive.hpp"
#include <cmath>
#include <iostream>
#include <unordered_set>
#include <istream>

std::unordered_map<unsigned char, std::string> createHuffmanMap(std::istream& file) {
    if (!file.good()) {
        std::cout << "File is not in good state. Returning empty map";
        return {};
    }
    const int beginning = file.tellg();

    std::unordered_set<unsigned char> bytes;
    char c;
    while (file.get(c)) {
        bytes.insert(static_cast<unsigned char>(static_cast<unsigned char>(c)));
    }
    int total = (bytes.size() <= 1) ? 1 : (int)std::ceil(std::log2(bytes.size()));

    std::unordered_map<unsigned char, std::string> map;
    int code = 0;
    for (unsigned char currentByte : bytes) {
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
    file.seekg(beginning);
    return map;
}


std::vector<unsigned char> encode(std::istream& file, const std::unordered_map<unsigned char, std::string> map) {
    if (!file.good()) {
        std::cout << "File is not in good state. Returning empty unsigned char vector";
        return {};
    }
    const int beginning = file.tellg();

    std::vector<unsigned char> encodedData;
    unsigned char buffer{0};
    int bitPos = 7;

    char c;
    while (file.get(c)) {

        const std::string& bits = map.at(static_cast<unsigned char>(c));

        for (char bit : bits) {
            if (bit == '1') {
                buffer |= static_cast<unsigned char>(1 << bitPos);
            }
            --bitPos;
            if (bitPos < 0) {
                encodedData.push_back(buffer);
                buffer = 0;
                bitPos = 7;
            }
        }
    }
    if (bitPos != 7) {
        encodedData.push_back(buffer);
    }

    file.clear();
    file.seekg(beginning);
    return encodedData;

}

std::vector<unsigned char> decode(std::istream& file, const std::unordered_map<unsigned char, std::string> map) {
    if (!file.good()) {
        std::cout << "File is not in good state. Returning empty unsigned char vector";
        return {};
    }
    const int beginning = file.tellg();

    std::unordered_map<std::string, unsigned char> reverseMap;
    for (const auto& [byte, bits] : map) {
        reverseMap[bits] = byte;
    }

    std::vector<unsigned char> decodedData;
    std::string current;
    char c;
    int total = map.empty() ? 0 : map.begin()->second.size();
    current.reserve(total);
    while (file.get(c)) {
        auto encodedByte = static_cast<unsigned char>(c);

        for (int i = 7; i >= 0; --i) {
            if ((encodedByte & static_cast<unsigned char>(1 << i)) != static_cast<unsigned char>(0)) {
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
    file.seekg(beginning);
    return decodedData;
}
