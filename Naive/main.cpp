#include <iostream>
#include <sstream>
#include <vector>
#include "huffman/naive.hpp"

using namespace std;

int main(){
	cout << "Enter a String to compress: ";
	string userInput;
	getline(cin, userInput);

	cout << "Original String size: " << userInput.size() << " bytes" << endl;

	istringstream inputData(userInput);
	auto huffmanMap = createHuffmanMap(inputData);

	vector<unsigned char> encodedData = encode(inputData, huffmanMap);
	string encodedStr;
	for (unsigned char b : encodedData) {
		encodedStr += static_cast<char>(b);
	}
	cout << "Compressed String size: " << encodedStr.size() << " bytes" << endl;

	istringstream encodedStream(encodedStr);
	vector<unsigned char> decodedData = decode(encodedStream, huffmanMap);
	string decodedStr;
	for (unsigned char b : decodedData) {
		decodedStr += static_cast<char>(b);
	}

	cout << "Decompressed String size: " << decodedData.size() << " bytes" << endl;
	cout << "Decompressed String: " << endl << decodedStr << endl;

	return 0;
}
