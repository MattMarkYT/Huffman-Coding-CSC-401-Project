#include "huffman/naive.hpp"
#include <cmath>
#include <iostream>
#include <unordered_set>
#include <istream>

using namespace std;

unordered_map<unsigned char, string> createHuffmanMap(istream& file) {
    if (!file.good()) {
        cout << "File is not in good state. Returning empty map";
        return {};
    }
    const int beginning = file.tellg();

    unordered_set<unsigned char> bytes;
    char c;
    while (file.get(c)) {
        bytes.insert(static_cast<unsigned char>(c));
    }
    int codeLength = (bytes.size() <= 1) ? 1 : (int)ceil(log2(bytes.size()));

    unordered_map<unsigned char, string> map;
    int code = 0;
    for (unsigned char currentByte : bytes) {
        string bits;
        for (int i = codeLength - 1; i >= 0; --i) {
            if ((code & (1 << i)) != 0) {
                bits += "1";
            } else {
                bits += "0";
            }
        }
        map[currentByte] = bits;
        ++code;
    }
    file.clear();
    file.seekg(beginning);
    return map;
}


vector<unsigned char> encode(istream& file, const unordered_map<unsigned char, string> map) {
    if (!file.good()) {
        cout << "File is not in good state. Returning empty unsigned char vector";
        return {};
    }
    const int beginning = file.tellg();

    vector<unsigned char> encodedData;
    unsigned char buffer{0};
    int bitPos = 7;

    char c;
    while (file.get(c)) {

        const string& bits = map.at(static_cast<unsigned char>(c));

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

vector<unsigned char> decode(istream& file, const unordered_map<unsigned char, string> map) {
    if (!file.good()) {
        cout << "File is not in good state. Returning empty unsigned char vector";
        return {};
    }
    const int beginning = file.tellg();

    const int codeLength = map.empty() ? 0 : static_cast<int>(map.begin()->second.length());
    unordered_map<unsigned char, unsigned char> reverseMap;
    for (const auto& [oldByte, oldCode] : map) {
        // converting string representation of code to byte
        unsigned char code = 0;
        unsigned char currentByte = codeLength - 1;
        for (const char bit : oldCode) {
            code |= (bit - 48) << currentByte;
            currentByte--;
        }

        reverseMap[code] = oldByte;
    }

    vector<unsigned char> decodedData;
    unsigned char encodedData;
    unsigned char code = 0;
    const unsigned char mask = static_cast<unsigned char>(255) >> (8 - codeLength);
    int shift = 8 - codeLength;
    while (file.get(reinterpret_cast<istream::char_type &>(encodedData))) {
        while (true) {
            code |= (encodedData >> shift) & mask;
            decodedData.push_back(reverseMap[code]);

            shift -= codeLength;
            if (shift < 0) {
                code = (encodedData << -shift) & mask;
                shift += 8;
                break;
            }
            code = 0;
        }
    }

    file.clear();
    file.seekg(beginning);
    return decodedData;
}