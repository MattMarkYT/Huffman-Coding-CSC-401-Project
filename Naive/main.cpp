#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include "huffman/naive.hpp"
#include "../BruhManager/include/bruhmanager.h"

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

	/////////// Files
	string filename = "bruh.txt";
	createFile(filename, encodedData, huffmanMap, userInput.size());

	istringstream encodedStream(encodedStr);
	cout << "test1" << endl;
	vector<unsigned char> decodedData = decode(encodedStream, huffmanMap);
	cout << "test2" << endl;
	string decodedStr;
	for (unsigned char b : decodedData) {
		decodedStr += static_cast<char>(b);
	}
	cout << "test3" << endl;

	cout << "Decompressed String size: " << decodedData.size() << " bytes" << endl;
	cout << "Decompressed String: " << endl << decodedStr << endl;

	////////////////// Files
	ifstream file("bruh.txt.nf", std::ios::binary);
	NaiveRecord* record = decodeNaiveFile(file);

	vector<unsigned char> decodedFile = decode(file, record->naiveMap);
	string decodedStr2(decodedData.begin(), decodedData.end());
	cout << "Decompressed String: " << endl; cout << decodedStr << endl;
	createFile("bruh.txt.nf", decodedFile);

	return 0;
}
