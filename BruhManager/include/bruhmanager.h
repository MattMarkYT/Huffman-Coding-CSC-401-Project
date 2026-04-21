#ifndef BRUHMANAGER_H
#define BRUHMANAGER_H
#include <string>
#include <unordered_map>
#include <vector>
#include "huffman/HuffNode.hpp"

using namespace std;

struct GreedyRecord {
    int ogLength;
    HuffNode* root;
};
struct NaiveRecord {
    int ogLength;
    unordered_map<unsigned char, string> naiveMap;
};

void createFile(const string& fileName, vector<unsigned char> decodedData);

void createFile(string& ogFileName, vector<unsigned char> encodedData, HuffNode* root, int ogLength);

void createFile(const string& ogFileName, vector<unsigned char> encodedData, unordered_map<unsigned char, string>& map, int ogLength);

GreedyRecord* decodeHuffmanFile(istream& file);

NaiveRecord* decodeNaiveFile(istream& file);

#endif //BRUHMANAGER_H