#ifndef LINKED_LIST_H
#define LINKED_LIST_H

// Singly-linked list template.
// Used by BudgetManager to store Expense, Income, and Bill records.
template <typename T>
class LinkedList {
private:
    struct Node {
        T data;
        Node* next;
        Node(const T& d) : data(d), next(nullptr) {}
    };
    Node* head;
    int   count;

public:
    LinkedList() : head(nullptr), count(0) {}

    ~LinkedList() {
        clear();
    }

    // Disable copy to keep the design simple (no deep-copy needed for this project).
    LinkedList(const LinkedList&) = delete;
    LinkedList& operator=(const LinkedList&) = delete;

    void pushFront(const T& value) {
        Node* n = new Node(value);
        n->next = head;
        head = n;
        ++count;
    }

    void pushBack(const T& value) {
        Node* n = new Node(value);
        if (!head) {
            head = n;
        } else {
            Node* cur = head;
            while (cur->next) cur = cur->next;
            cur->next = n;
        }
        ++count;
    }

    // Remove the first element matching the predicate. Returns true if removed.
    template <typename Pred>
    bool removeIf(Pred pred) {
        Node* prev = nullptr;
        Node* cur  = head;
        while (cur) {
            if (pred(cur->data)) {
                if (prev) prev->next = cur->next;
                else      head       = cur->next;
                delete cur;
                --count;
                return true;
            }
            prev = cur;
            cur  = cur->next;
        }
        return false;
    }

    // Iterate every element with a callable f(const T&) or f(T&).
    template <typename F>
    void forEach(F f) {
        Node* cur = head;
        while (cur) {
            f(cur->data);
            cur = cur->next;
        }
    }

    template <typename F>
    void forEach(F f) const {
        Node* cur = head;
        while (cur) {
            f(cur->data);
            cur = cur->next;
        }
    }

    // Find first element matching pred. Returns nullptr if none.
    template <typename Pred>
    T* find(Pred pred) {
        Node* cur = head;
        while (cur) {
            if (pred(cur->data)) return &cur->data;
            cur = cur->next;
        }
        return nullptr;
    }

    int  size()    const { return count; }
    bool isEmpty() const { return head == nullptr; }

    void clear() {
        while (head) {
            Node* n = head;
            head = head->next;
            delete n;
        }
        count = 0;
    }
};

#endif
