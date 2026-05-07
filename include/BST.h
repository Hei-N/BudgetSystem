#ifndef BST_H
#define BST_H

#include <vector>

// Binary Search Tree keyed by a comparable Key.
// In this project it stores Expense pointers keyed by Date for fast range queries
// such as "all expenses from March 1 to March 15" via in-order range traversal.
template <typename Key, typename Value>
class BST {
private:
    struct Node {
        Key key;
        Value value;
        Node* left;
        Node* right;
        Node(const Key& k, const Value& v) : key(k), value(v), left(nullptr), right(nullptr) {}
    };

    Node* root;
    int   count;

    Node* insertNode(Node* node, const Key& k, const Value& v) {
        if (!node) { ++count; return new Node(k, v); }
        if (k < node->key)        node->left  = insertNode(node->left,  k, v);
        else                      node->right = insertNode(node->right, k, v);
        return node;
    }

    void inOrder(Node* node, std::vector<std::pair<Key, Value>>& out) const {
        if (!node) return;
        inOrder(node->left, out);
        out.push_back({ node->key, node->value });
        inOrder(node->right, out);
    }

    void rangeQuery(Node* node, const Key& lo, const Key& hi,
                    std::vector<std::pair<Key, Value>>& out) const {
        if (!node) return;
        if (lo < node->key)      rangeQuery(node->left,  lo, hi, out);
        if (!(node->key < lo) && !(hi < node->key)) out.push_back({ node->key, node->value });
        if (node->key < hi)      rangeQuery(node->right, lo, hi, out);
    }

    void destroy(Node* node) {
        if (!node) return;
        destroy(node->left);
        destroy(node->right);
        delete node;
    }

public:
    BST() : root(nullptr), count(0) {}
    ~BST() { destroy(root); }

    BST(const BST&) = delete;
    BST& operator=(const BST&) = delete;

    void insert(const Key& key, const Value& value) {
        root = insertNode(root, key, value);
    }

    // Returns all entries with keys in [lo, hi] (inclusive).
    std::vector<std::pair<Key, Value>> range(const Key& lo, const Key& hi) const {
        std::vector<std::pair<Key, Value>> out;
        rangeQuery(root, lo, hi, out);
        return out;
    }

    // Sorted in-order traversal of every entry.
    std::vector<std::pair<Key, Value>> inOrderAll() const {
        std::vector<std::pair<Key, Value>> out;
        inOrder(root, out);
        return out;
    }

    int  size()    const { return count; }
    bool isEmpty() const { return root == nullptr; }
};

#endif
