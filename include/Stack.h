#ifndef STACK_H
#define STACK_H

// Simple linked-list backed stack used for the undo feature.
template <typename T>
class Stack {
private:
    struct Node {
        T data;
        Node* next;
        Node(const T& d) : data(d), next(nullptr) {}
    };
    Node* topNode;
    int   count;

public:
    Stack() : topNode(nullptr), count(0) {}
    ~Stack() { clear(); }

    Stack(const Stack&) = delete;
    Stack& operator=(const Stack&) = delete;

    void push(const T& value) {
        Node* n = new Node(value);
        n->next = topNode;
        topNode = n;
        ++count;
    }

    bool pop(T& out) {
        if (!topNode) return false;
        Node* n = topNode;
        out      = n->data;
        topNode  = n->next;
        delete n;
        --count;
        return true;
    }

    bool peek(T& out) const {
        if (!topNode) return false;
        out = topNode->data;
        return true;
    }

    bool isEmpty() const { return topNode == nullptr; }
    int  size()    const { return count; }

    void clear() {
        while (topNode) {
            Node* n = topNode;
            topNode = topNode->next;
            delete n;
        }
        count = 0;
    }
};

#endif
