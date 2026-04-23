#include <sstream>
#include <iostream>
#include <string>
#include <fstream>
#include <queue>
#include <filesystem>
#include "../Greedy/include/huffman/greedy.hpp"
#include "../Naive/include/huffman/naive.hpp"
#include "../TimeKeeper/include/huffman/timekeeper.h"
#include "huffman/FileManager.hpp"
#include "huffman/FMMisc.hpp"
using namespace std;


struct File {
	string path;
	string filename;
	size_t size;
};

void batchTest(bool isGreedy) {
	int count = 0;
	string inputFolder = "input";
	queue<File> fileQueue;


	if (!filesystem::exists(inputFolder) || !filesystem::is_directory(inputFolder)) {
		cout << "Input folder not found." << endl;
		return;
	} // simple check for directory

	for (const auto& entry : filesystem::directory_iterator(inputFolder)) {
		if (entry.is_regular_file()) {
			File current;
			current.path      = entry.path().string();
			current.filename  = entry.path().filename().string();
			current.size      = entry.file_size();
			fileQueue.push(current);
		}
	} // this is where we go file by file, read some of its basic stats and push it to queue.

	cout << "\nFound " << fileQueue.size() << " file(s) in '" << inputFolder << "'" << endl;
	string outputName = string("output_") + (isGreedy ? "greedy" : "naive") + ".csv";
	ofstream output(outputName, ios::trunc);
	if (!output.is_open()) {
		cout << "Could not create CSV file." << endl;
		return;
	}
	output << "Filename,Mode,Original Size,Compressed Size,Decoded Size,Tree / Map Creation (ms),Encoding (ms),Decoding (ms),Verified\n";

	// this is where we process each file.
	while (!fileQueue.empty()) {
		File currentFile = fileQueue.front();
		fileQueue.pop();
		ifstream file(currentFile.path, ios::binary);

		if (!file.is_open()) {
			cout << "Could not open file. Skipping." << endl;
			continue;
		}

		if (isGreedy) {
			resetTimer();
			startTimer();
			HuffNode* root = createHuffmanTree(file);
			stopTimer();
			saveTimer("Tree");


			resetTimer();
			startTimer();
			vector<unsigned char> encodedData = encode(file, root);
			stopTimer();
			saveTimer("Encoding");

			FileManager manager;
			FMErrorCode writeResult = manager.writeFormat(currentFile.filename, encodedData, currentFile.size, root, false, true);
			if (writeResult != FMErrorCode::none) {
				cout << "writeFormat failed, skipping." << endl;
				deleteTree(root);
				continue;
			}

			manager.openFileR(currentFile.filename + ".hhuf", FMFileType::huffman, false);
			HuffNode* readRoot = nullptr;
			manager.parseDictionary(readRoot, false);
			manager.jumpToData(false);
			uint64_t decodedLength = manager.getDecodedPayloadLength();
			shared_ptr<fstream> encodedStream = manager.detachStream();

			resetTimer();
			startTimer();
			vector<unsigned char> decodedData = decode(*encodedStream, readRoot, (size_t)decodedLength);
			stopTimer();
			saveTimer("Decoding");

			bool isVerified = (decodedData.size() == currentFile.size);

			output << currentFile.filename << ","
				  << "Greedy,"
				  << currentFile.size << ","
				  << encodedData.size() << ","
			      << decodedData.size() << ","
				  << timerGetTotalMilli("Tree") << ","
				  << timerGetTotalMilli("Encoding") << ","
				  << timerGetTotalMilli("Decoding") << ","
				  << (isVerified ? "YES" : "NO") << "\n";
			count++;

			deleteTree(readRoot);
			deleteTree(root);
		} else {
			resetTimer();
			startTimer();
			auto huffmanMap = createHuffmanMap(file);
			stopTimer();
			saveTimer("Map");

			resetTimer();
			startTimer();
			vector<unsigned char> encodedData = encode(file, huffmanMap);
			stopTimer();
			saveTimer("Encoding");

			FileManager manager;
			FMErrorCode writeResult = manager.writeFormat(currentFile.filename, encodedData, currentFile.size, huffmanMap, false, true);
			if (writeResult != FMErrorCode::none) {
				cout << "writeFormat failed, skipping." << endl;
				continue;
			}
			manager.openFileR(currentFile.filename + ".hnai", FMFileType::naive, false);
			manager.parseDictionary(huffmanMap, false);
			manager.jumpToData(false);
			uint64_t decodedLength = manager.getDecodedPayloadLength();
			shared_ptr<fstream> encodedStream = manager.detachStream();

			resetTimer();
			startTimer();
			vector<unsigned char> decodedData = decode(*encodedStream, huffmanMap, (size_t)decodedLength);
			stopTimer();
			saveTimer("Decoding");

			bool isVerified = (decodedData.size() == currentFile.size);

			output << currentFile.filename << ","
				   << "Naive,"
				   << currentFile.size << ","
				   << encodedData.size() << ","
			       << decodedData.size() << ","
				   << timerGetTotalMilli("Map") << ","
				   << timerGetTotalMilli("Encoding") << ","
				   << timerGetTotalMilli("Decoding") << ","
				   << (isVerified ? "YES" : "NO") << "\n";
			count++;
		}
		file.close();
	}

	cout << "Processed " << count << " Files" << endl;
}

void greedyManual() {
	cout << "\nHello Greedy!" << endl;
	cout << "Enter a String to compress: ";

	string userInput;
	getline(cin, userInput);

	cout << "\nOriginal String size: "; cout << userInput.size(); cout << " bytes" << endl;

	istringstream inputData(userInput);

	HuffNode* root = createHuffmanTree(inputData);

	vector<unsigned char> encodedData = encode(inputData, root);
	string encodedStr(encodedData.begin(), encodedData.end());

	cout << "Compressed String size: "; cout << encodedStr.size(); cout << " bytes" << endl;


	istringstream encodedInputData(encodedStr);

	vector<unsigned char> decodedData = decode(encodedInputData, root, userInput.size());
	string decodedStr(decodedData.begin(), decodedData.end());

	cout << "Decompressed String size: "; cout << decodedData.size(); cout << " bytes" << endl;
	cout << "Decompressed String: " << endl; cout << decodedStr << endl;
	deleteTree(root);
}

void naiveManual() {
	cout << "\nHello Naive!" << endl;
	cout << "Enter a String to compress: ";
	string userInput;
	getline(cin, userInput);

	cout << "\nOriginal String size: " << userInput.size() << " bytes" << endl;

	istringstream inputData(userInput);
	auto huffmanMap = createHuffmanMap(inputData);

	vector<unsigned char> encodedData = encode(inputData, huffmanMap);
	string encodedStr(encodedData.begin(), encodedData.end());
	cout << "Compressed String size: " << encodedStr.size() << " bytes" << endl;

	istringstream encodedStream(encodedStr);
	vector<unsigned char> decodedData = decode(encodedStream, huffmanMap,  userInput.size());
	string decodedStr(decodedData.begin(), decodedData.end());

	cout << "Decompressed String size: " << decodedData.size() << " bytes" << endl;
	cout << "Decompressed String: " << endl << decodedStr << endl;
}





void runSuite() {
	int choice = 0;
	bool isGreedy = false;

	while (true) {
		cout << "\nWelcome to Huffman Test Suite" << endl;
		cout << "1. Run Manual Input Test" << endl;
		cout << "2. Run File Test (Refer to Input folder)" << endl;
		if (!isGreedy) { cout << "3. Switch Mode (Currently using Naive)" << endl; }
		if (isGreedy)  { cout << "3. Switch Mode (Currently using Greedy)" << endl; }
		cout << "4. Exit" << endl << endl;
		cout << "Enter your choice: ";

		if (!(cin >> choice)) {
			cin.clear();
			cin.ignore(numeric_limits<streamsize>::max(), '\n');
			continue;
		}

		cin.ignore(numeric_limits<streamsize>::max(), '\n');

		switch (choice) {
			case 1:
				if (isGreedy) {
					greedyManual();
				} else {
					naiveManual();
				}
				break;
			case 2:
				batchTest(isGreedy);
				break;
			case 3:
				isGreedy = !isGreedy;
				cout << "\nMode Switched" << endl;
				break;
			case 4:
				return;
			default:
				cout << "Invalid choice. Please try again." << endl;
				break;
		}
	}
}

int main() {
	runSuite();
	return 0;
}