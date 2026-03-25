#include "huffman/greedy.hpp"

#include <iostream>
#include <istream>
#include <queue>

using namespace std;

void generateCodes(unordered_map<unsigned char, string>&, HuffNode*, string);

HuffNode* createHuffmanTree(istream& file) {
    if (!file.good()) {
        cout << "File is not in good state. Returning nullptr";
        return nullptr;
    }

    // I'm going to loop through the stream and collect frequencies
    // Each time I run into a char, I'll increment the frequency by 1.
    char c;
    unordered_map<unsigned char, int> frequencies; // (key:char, value:frequency)

    while (file.get(c)) { // gets the next byte and puts it in c
        auto pair = frequencies.find(c); // This grabs the key-value pair. Variable "first" is the key. "second" is the value. If not found, pair->second == frequencies.end() (end of the map object)
        frequencies.insert_or_assign(c, (pair == frequencies.end() ? 0 : pair->second) + 1); // If pair == frequencies.end(), use 0, else use the value
    }

    // Time to build the tree
    priority_queue<HuffNode*, vector<HuffNode*>, CompareHuffNodePtr> pq;
    // Create nodes from the char, frequency pairs and put them in the pq
    for (const auto& pair : frequencies) {
        pq.push(new HuffNode(pair.first, pair.second));
    }

    // Combine until we're left with 1 node as the root
    while (pq.size() > 1) {
        HuffNode* l = pq.top(); pq.pop();
        HuffNode* r = pq.top(); pq.pop();

        pq.push(new HuffNode(l->frequency+r->frequency, l, r));

    }
    HuffNode* root = pq.top(); pq.pop();   // Pointers make me paranoid

    // Move file pointer back to the beginning
    file.clear();    // Clear error flags from reaching the end
    file.seekg(0);   // return pointer to beginnning of file.

    return root;
}

vector<unsigned char> encode(istream& file, HuffNode* tree) {
    // Generate huffman codes as strings to use during encoding
    unordered_map<unsigned char, string> codes;
    generateCodes(codes, tree, "");


    // I'm going to encode data into encodedData as I'm reading it from the file.
    vector<unsigned char> encodedData;
    unsigned char buffer = 0;
    char nextByte;
    int onBit = 7;
    // For each byte in the file
    while (file.get(nextByte)) {
        string str = codes.find(nextByte)->second;
        // For each character in the string
        for (char c: str) {
            buffer |= static_cast<unsigned char>((c == '1' ? 1 : 0) << onBit);
            onBit--;
            if (onBit < 0) {		// We hit the right of our buffer, move on to the next one
                encodedData.push_back(buffer);
                onBit = 7; buffer = 0;
            }
        }
    }
    // The loop only pushes when the buffer is full
    // If we don't do this, some information would be truncated
    if (onBit != 7) 
        encodedData.push_back(buffer);

    // Move file pointer back to the beginning
    file.clear();    // Clear error flags from reaching the end
    file.seekg(0);   // return pointer to beginnning of file.

    return encodedData;
}

vector<unsigned char> decode(istream& file, HuffNode* root) {
    vector<unsigned char> decodedData;
    char currentByte;
    HuffNode* node = root;
    while (file.get(currentByte)) {
        for (int onBit = 7; onBit >= 0; onBit--) {     // Reading bits from left to right
            if ((currentByte & (1 << onBit)) == 0) {   // If current bit is 0,
                node = node->left;                     // go left.
                if (node->left == nullptr) {           // If that node is a deadend,
                    decodedData.push_back(node->data); // Get the decoded byte
                    node = root;                       // and go back to root.
                }
            }
            else {                                     // else
                node = node->right;                    // go right.
                if (node->right == nullptr) {          // If that node is a deadend,
                    decodedData.push_back(node->data); // Get the decoded byte
                    node = root;                       // and go back to root.
                }
            }
        }
    }

    // Move file pointer back to the beginning
    file.clear();    // Clear error flags from reaching the end
    file.seekg(0);   // return pointer to beginnning of file.

    return decodedData;
}

// Recursive function to generate codes using preorder traversal
void generateCodes(unordered_map<unsigned char, string>& map, HuffNode* node, string str) {
    if (node->left != nullptr) {
        generateCodes(map, node->left, str+"0");
    }
    else {
        map.insert_or_assign(node->data, str);
        cout << node->data << " | " + str << endl;
        return;
    }
    if (node->right != nullptr) {
        generateCodes(map, node->right, str+"1");
    }
    else {
        map.insert_or_assign(node->data, str);
        cout << node->data << " | " + str << endl;
    }
}
