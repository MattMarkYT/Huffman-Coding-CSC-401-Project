#include <iostream>
#include <sstream>
#include <vector>

#include "huffman/greedy.hpp"
#include "huffman/HuffNode.hpp"

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

	istringstream encodedInputData(encodedStr);

	vector<unsigned char> decodedData = decode(encodedInputData, root);
	string decodedStr(decodedData.begin(), decodedData.end());

	cout << "Decompressed String size: "; cout << decodedData.size(); cout << " bytes" << endl;
	cout << "Decompressed String: " << endl; cout << decodedStr << endl;

	return 0;
}
