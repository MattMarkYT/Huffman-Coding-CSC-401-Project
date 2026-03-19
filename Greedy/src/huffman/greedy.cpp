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

    char c;
    unordered_map<unsigned char, int> frequencies;

    while (file.get(c)) {
        auto value = frequencies.find(c);
        frequencies.insert_or_assign(c, (value != frequencies.end() ? value->second : 0) + 1);
    }
    priority_queue<HuffNode*, vector<HuffNode*>, CompareHuffNodePtr> pq;

    for (const auto& pair : frequencies) {
        pq.push(new HuffNode(pair.first, pair.second));
    }

    while (pq.size() > 1) {
        HuffNode* l = pq.top(); pq.pop();
        HuffNode* r = pq.top(); pq.pop();

        pq.push(new HuffNode(l->frequency+r->frequency, l, r));

    }
    HuffNode* h = pq.top(); pq.pop();   // Pointers make me paranoid

    file.clear();
    file.seekg(0);

    return h;
}

vector<unsigned char> encode(istream& file, HuffNode* tree) {
    unordered_map<unsigned char, string> codes;
    generateCodes(codes, tree, "");

    vector<unsigned char> encodedData;
    unsigned char buffer = 0;
    char nextByte;
    int onBit = 7;
    while (file.get(nextByte)) {
        string str = codes.find(nextByte)->second;
        for (char c: str) {
            buffer |= static_cast<unsigned char>((c == '1' ? 1 : 0) << onBit);
            onBit--;
            if (onBit < 0) {		// We hit the right of the byte, move on to the next one
                encodedData.push_back(buffer);
                onBit = 7; buffer = 0;
            }
        }
    }
    if (onBit != 7)
        encodedData.push_back(buffer);

    file.clear();
    file.seekg(0);

    return encodedData;
}

vector<unsigned char> decode(istream& file, HuffNode* root) {
    vector<unsigned char> decodedData;
    char currentByte;
    HuffNode* node = root;
    while (file.get(currentByte)) {
        for (int onBit = 7; onBit >= 0; onBit--) {
            if ((currentByte & (1 << onBit)) == 0) {
                node = node->left;
                if (node->left == nullptr) {
                    decodedData.push_back(node->data);
                    node = root;
                }
            }
            else {
                node = node->right;
                if (node->right == nullptr) {
                    decodedData.push_back(node->data);
                    node = root;
                }
            }
        }
    }

    file.clear();
    file.seekg(0);

    return decodedData;
}

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