#ifndef GREEDY_HPP
#define GREEDY_HPP
#include <cstddef>
#include <iosfwd>
#include <unordered_map>
#include <vector>

#include "huffman/HuffNode.hpp"

HuffNode* createHuffmanTree(std::istream& file);

std::vector<unsigned char> encode(std::istream& file, HuffNode* tree);

std::vector<unsigned char> decode(std::istream& file, HuffNode* root, size_t originalSize);

#endif // GREEDY_HPP
