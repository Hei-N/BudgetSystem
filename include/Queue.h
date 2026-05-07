#ifndef QUEUE_H
#define QUEUE_H

// FIFO queue used by the ReminderSystem to process upcoming bills in order.
template <typename T>
class Queue {
private:
    struct Node {
        T data;
        Node* next;
        Node(const T& d) : data(d), next(nullptr) {}
    };
    Node* frontNode;
    Node* backNode;
    int   count;

public:
    Queue() : frontNode(nullptr), backNode(nullptr), count(0) {}
    ~Queue() { clear(); }

    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    void enqueue(const T& value) {
        Node* n = new Node(value);
        if (!backNode) {
            frontNode = backNode = n;
        } else {
            backNode->next = n;
            backNode = n;
        }
        ++count;
    }

    bool dequeue(T& out) {
        if (!frontNode) return false;
        Node* n = frontNode;
        out = n->data;
        frontNode = n->next;
        if (!frontNode) backNode = nullptr;
        delete n;
        --count;
        return true;
    }

    template <typename F>
    void forEach(F f) const {
        Node* cur = frontNode;
        while (cur) {
            f(cur->data);
            cur = cur->next;
        }
    }

    bool isEmpty() const { return frontNode == nullptr; }
    int  size()    const { return count; }

    void clear() {
        while (frontNode) {
            Node* n = frontNode;
            frontNode = frontNode->next;
            delete n;
        }
        backNode = nullptr;
        count = 0;
    }
};

#endif
