#include "huffman/naive.hpp"
#include <unordered_set>

// The returned map should be bytes and their string representation e.g. j -> "0001"
// With Naive, each character uses the same amount of bits
// You can use c++'s unordered set to find all unique characters in the file
// USE MALLOC OR THE MAP WILL BE DESTROYED DUE TO SCOPE
std::unordered_map<std::byte, std::string> createHuffmanMap(std::istream& file) {

    // return ; // DO NOT FORGET
}

// Create a std::vector<std::byte> and a temp byte
// You have the outer loop reading the file. inner loop going through the string, writing the encoded data into the temp byte.
// After each character, you should check if you've reached the end of the byte. If so, push it into the vector.
// vector is kind of like java's ArrayList. Look into its functions online. (push_back function)
// vector does not require malloc and will not be destroyed from scope if returned.
std::vector<std::byte> encode(std::istream& file, const std::unordered_map<std::byte, std::string> map) {

    // return ; // DO NOT FORGET
}

// Here you will just do the reverse of what you did in encode
std::vector<std::byte> decode(std::istream& file, const std::unordered_map<std::byte, std::string> map) {

    // return ; // DO NOT FORGET
}
