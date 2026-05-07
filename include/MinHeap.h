#ifndef MIN_HEAP_H
#define MIN_HEAP_H

#include <vector>

// Generic min-heap. Comparator returns true if a "comes before" b (i.e. a is "smaller").
// Used by ReminderSystem to surface the bill with the earliest due date in O(log n).
template <typename T, typename Cmp>
class MinHeap {
private:
    std::vector<T> data;
    Cmp cmp;

    void heapifyUp(int i) {
        while (i > 0) {
            int parent = (i - 1) / 2;
            if (cmp(data[i], data[parent])) {
                std::swap(data[i], data[parent]);
                i = parent;
            } else break;
        }
    }

    void heapifyDown(int i) {
        int n = static_cast<int>(data.size());
        while (true) {
            int l = 2 * i + 1;
            int r = 2 * i + 2;
            int smallest = i;
            if (l < n && cmp(data[l], data[smallest])) smallest = l;
            if (r < n && cmp(data[r], data[smallest])) smallest = r;
            if (smallest != i) {
                std::swap(data[i], data[smallest]);
                i = smallest;
            } else break;
        }
    }

public:
    explicit MinHeap(Cmp c = Cmp()) : cmp(c) {}

    void push(const T& value) {
        data.push_back(value);
        heapifyUp(static_cast<int>(data.size()) - 1);
    }

    bool peek(T& out) const {
        if (data.empty()) return false;
        out = data.front();
        return true;
    }

    bool pop(T& out) {
        if (data.empty()) return false;
        out = data.front();
        data.front() = data.back();
        data.pop_back();
        if (!data.empty()) heapifyDown(0);
        return true;
    }

    // Rebuild the heap after external mutation (e.g. a bill was marked paid).
    void rebuild() {
        for (int i = static_cast<int>(data.size()) / 2 - 1; i >= 0; --i) heapifyDown(i);
    }

    template <typename F>
    void forEach(F f) const { for (const T& v : data) f(v); }

    int  size()    const { return static_cast<int>(data.size()); }
    bool isEmpty() const { return data.empty(); }
    void clear()         { data.clear(); }
};

#endif
