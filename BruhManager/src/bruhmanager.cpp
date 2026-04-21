#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>

#include "huffman/HuffNode.hpp"
#include "../include/bruhmanager.h"

#include <cmath>
#include <iostream>
#include <istream>

using namespace std;

void encodeNode(const HuffNode* node, string* output) {
    if (node->data) {
        output->push_back('1');
        output->push_back(static_cast<char>(node->data));
    }
    else {
        output->push_back('0');
        encodeNode(node->left, output);
        encodeNode(node->right, output);
    }
}


void createFile(const string& fileName, vector<unsigned char> decodedData) {
    std::ofstream outputFile(fileName.substr(0, fileName.size()-3),ios::binary);
    if (!outputFile.is_open()) {
        std::cerr << "Unable to open file"; return;
    }
    copy(decodedData.begin(), decodedData.end(), std::ostreambuf_iterator<char>(outputFile));
    outputFile.close();
}

void createFile(string& ogFileName, vector<unsigned char> encodedData, HuffNode* root, int ogLength) {
    vector<unsigned char> treeBytes;
    string treeStr;
    encodeNode(root, &treeStr);
    cout << treeStr << endl;

    unsigned char byte = 0;
    int shift = 0;
    for (char bit : treeStr) {
        byte |= (bit-48) << shift;
        shift++;
        if (shift == 8) {shift = 0; treeBytes.push_back(static_cast<unsigned char>(byte));}
    }
    if (shift != 0) {
        treeBytes.push_back(byte);
    }

    std::ofstream outputFile(ogFileName+".hf",ios::binary);
    if (!outputFile.is_open()) {
        std::cerr << "Unable to open file"; return;
    }

    outputFile.write(reinterpret_cast<const char*>(&ogLength), sizeof(ogLength));
    copy(treeBytes.begin(), treeBytes.end(), std::ostreambuf_iterator<char>(outputFile));
    copy(encodedData.begin(), encodedData.end(), std::ostreambuf_iterator<char>(outputFile));

    outputFile.close();
}

void createFile(const string& ogFileName, vector<unsigned char> encodedData, unordered_map<unsigned char, string>& map, int ogLength) {
    unsigned char numCodes = map.size();
    cout << "numCodes: " << static_cast<int>(numCodes) << endl;
    unsigned char codeLength = (numCodes <= 1) ? 1 : static_cast<int>(ceil(log2(numCodes)));

    vector<unsigned char> finalMap{};
    unordered_map<unsigned char, unsigned char> reverseMap;
    for (const auto& [oldByte, oldCode] : map) {
        unsigned char code = 0;
        unsigned char currentByte = codeLength - 1;
        for (const char bit : oldCode) {
            code |= (bit - 48) << currentByte;
            currentByte--;
        }
        reverseMap[code] = oldByte;
    }
    for (int i = 0; i < codeLength; i++) {
        finalMap.push_back(reverseMap.at(i));
    }

    std::ofstream outputFile(ogFileName+".nf",ios::binary);
    if (!outputFile.is_open()) {
        std::cerr << "Unable to open file"; return;
    }

    outputFile.write(reinterpret_cast<const char*>(&ogLength), sizeof(ogLength));
    outputFile.write(reinterpret_cast<const ostream::char_type *>(&numCodes), sizeof(numCodes));
    copy(finalMap.begin(), finalMap.end(), std::ostreambuf_iterator<char>(outputFile));
    copy(encodedData.begin(), encodedData.end(), std::ostreambuf_iterator<char>(outputFile));

    outputFile.close();
}

unsigned char readBit(istream* file, const int onBit) {
    const unsigned char c = 1 & file->get() >> onBit;
    if (onBit < 7) file->unget();
    return c;
}
unsigned char readByte(istream* file, const int onBit) {
    const int shift = 8-onBit;
    const unsigned char mask = 255 >> shift;
    unsigned char c = file->get() << onBit;

    if (onBit > 0) {
        c |= mask & file->get() >> shift;
        file->unget();
    }
    return c;
}

HuffNode* decodeNode(istream* file, int* onBit) {
    const unsigned char bit = readBit(file, *onBit);
    *onBit += 1;

    if (bit == 1) {
        const unsigned char value = readByte(file, *onBit);
        return new HuffNode(value, 0);
    }
    else {
        HuffNode* left = decodeNode(file, onBit);
        HuffNode* right = decodeNode(file, onBit);
        return new HuffNode(0, left, right);
    }
}

GreedyRecord* decodeHuffmanFile(istream& file) {
    auto* record = new GreedyRecord();

    int ogLength = 0;
    file.read(reinterpret_cast<istream::char_type*>(&ogLength), sizeof(int));
    record->ogLength = ogLength;

    cout << "ogLength: " << ogLength << endl;

    int onBit = 0;
    HuffNode* root = decodeNode(&file, &onBit);
    record->root = root;

    return record;
}

NaiveRecord* decodeNaiveFile(istream& file) {
    auto* record = new NaiveRecord();

    int ogLength = 0;
    file.read(reinterpret_cast<istream::char_type*>(&ogLength), sizeof(int));
    record->ogLength = ogLength;

    unordered_map<unsigned char, string> decodedMap{};

    const unsigned char numCodes = file.get();
    const unsigned char codeLength = (numCodes <= 1) ? 1 : static_cast<int>(ceil(log2(numCodes)));
    int bitsLeft = codeLength;
    string code;
    unsigned char onCode = 0;
    const unsigned char byte = file.get();
    while (onCode < numCodes) {
        for (int onBit = 7; onBit >= 0; onBit--) {
            const unsigned char bit = 1 & byte >> onBit;
            code += (bit << bitsLeft) == 0 ? "0" : "1";
            bitsLeft--;
            if (bitsLeft == 0) {
                bitsLeft = codeLength;
                decodedMap[onCode] = code;
                code = ""; onCode++;
            }
        }
    }

    record->naiveMap = decodedMap;

    return record;
}