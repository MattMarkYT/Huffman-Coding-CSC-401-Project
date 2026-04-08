#include <iostream>
#include <sstream>
#include <vector>
#include "huffman/naive.hpp"

int main(){
	std::cout << "Enter a String to compress: ";
	std::string userInput;
	std::cin >> userInput;

	std::cout << "Original String size: " << userInput.size() << " bytes" << std::endl;

	std::istringstream inputData(userInput);
	auto huffmanMap = createHuffmanMap(inputData);

	std::vector<std::byte> encodedData = encode(inputData, huffmanMap);
	std::string encodedStr;
	for (std::byte b : encodedData) {
		encodedStr += static_cast<char>(std::to_integer<unsigned char>(b));
	}
	std::cout << "Compressed String size: " << encodedStr.size() << " bytes" << std::endl;

	std::istringstream encodedStream(encodedStr);
	std::vector<std::byte> decodedData = decode(encodedStream, huffmanMap);
	std::string decodedStr;
	for (std::byte b : decodedData) {
		decodedStr += static_cast<char>(std::to_integer<unsigned char>(b));
	}

	std::cout << "Decompressed String size: " << decodedData.size() << " bytes" << std::endl;
	std::cout << "Decompressed String: " << std::endl << decodedStr << std::endl;

	return 0;
}
