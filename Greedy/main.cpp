#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "huffman/greedy.hpp"
#include "huffman/HuffNode.hpp"
#include "../BruhManager/include/bruhmanager.h"

using namespace std;

int main(){
	cout << "Hello Greedy!" << endl;
	cout << "Enter a String to compress: ";

	string userInput;
	getline(cin, userInput);

	cout << "Original String size: "; cout << userInput.size(); cout << " bytes" << endl;

	istringstream inputData(userInput);

	HuffNode* root = createHuffmanTree(inputData);

	vector<unsigned char> encodedData = encode(inputData, root);
	string encodedStr(encodedData.begin(), encodedData.end());

	cout << "Compressed String size: "; cout << encodedStr.size(); cout << " bytes" << endl;
	//cout << "Compressed String: " << endl; cout << encodedStr << endl;

	/////////// Files
	string filename = "bruh.txt";
	createFile(filename, encodedData, root, userInput.size());

	istringstream encodedInputData(encodedStr);

	vector<unsigned char> decodedData = decode(encodedInputData, root, userInput.size());
	string decodedStr(decodedData.begin(), decodedData.end());

	cout << "Decompressed String size: "; cout << decodedData.size(); cout << " bytes" << endl;
	cout << "Decompressed String: " << endl; cout << decodedStr << endl;

	////////////////// Files
	ifstream file("bruh.txt.hf", std::ios::binary);
	GreedyRecord* record = decodeHuffmanFile(file);

	cout << "ogLength: " << record->ogLength << " | Root not null: " << (record->root != nullptr) << endl;

	vector<unsigned char> decodedFile = decode(file, record->root, record->ogLength);
	string decodedStr2(decodedData.begin(), decodedData.end());
	cout << "Decompressed String: " << endl; cout << decodedStr << endl;
	createFile("bruh.txt.hf", decodedFile);

	return 0;
}
