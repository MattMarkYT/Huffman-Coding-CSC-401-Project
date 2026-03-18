#ifndef NAIVE_HPP
#define NAIVE_HPP
#include <string>
#include <unordered_map>
#include <vector>

std::unordered_map<std::byte, std::string> createHuffmanMap(std::istream& file);

std::vector<std::byte> encode(std::istream& file, std::unordered_map<std::byte, std::string> map);

std::vector<std::byte> decode(std::istream& file, std::unordered_map<std::byte, std::string> map);

#endif // NAIVE_HPP
