#ifndef NAIVE_HPP
#define NAIVE_HPP
#include <string>
#include <unordered_map>
#include <vector>

std::unordered_map<unsigned char, std::string> createHuffmanMap(std::istream& file);

std::vector<unsigned char> encode(std::istream& file, std::unordered_map<unsigned char, std::string> map);

std::vector<unsigned char> decode(std::istream& file, std::unordered_map<unsigned char, std::string> map);

#endif // NAIVE_HPP
