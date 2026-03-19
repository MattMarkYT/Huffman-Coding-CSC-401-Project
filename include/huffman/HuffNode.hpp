#ifndef HUFFNODE_HPP
#define HUFFNODE_HPP

struct HuffNode {

    HuffNode(unsigned char d, int f) : data(d), frequency(f), left(nullptr), right(nullptr) {}
    HuffNode(int f, HuffNode* l, HuffNode* r) : frequency(f), left(l), right(r) {}

    unsigned char data;
    int frequency;
    HuffNode* left;
    HuffNode* right;

    bool operator<(const HuffNode& other) const {
        return frequency > other.frequency;
    }

};

struct CompareHuffNodePtr {
    bool operator()(const HuffNode* first, const HuffNode* second) const {
        // Dereference and compare the actual values
        return first->frequency > second->frequency;
    }
};

#endif // HUFFNODE_HPP
