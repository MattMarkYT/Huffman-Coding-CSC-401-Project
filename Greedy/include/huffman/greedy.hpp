#ifndef GREEDY_HPP
#define GREEDY_HPP
#include <cstddef>
#include <iosfwd>
#include <unordered_map>
#include <vector>

#include "huffman/HuffNode.hpp"

HuffNode* createHuffmanTree(std::istream& file);

std::vector<std::byte> encode(std::istream& file, HuffNode* tree);

std::vector<std::byte> decode(std::istream& file, HuffNode* tree);

#endif // GREEDY_HPP
