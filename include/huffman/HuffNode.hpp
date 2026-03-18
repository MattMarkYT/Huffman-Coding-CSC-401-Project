
struct HuffNode {

    HuffNode(std::byte d, int f) : data(d), frequency(f) {}
    HuffNode(int f, HuffNode* l, HuffNode* r) : frequency(f), left(l), right(r) {}

    std::byte data;
    int frequency;
    HuffNode* left;
    HuffNode* right;

    bool operator<(const HuffNode& other) const {
        return frequency > other.frequency;
    }

};