#ifndef HASH_MAP_H
#define HASH_MAP_H

#include <string>
#include <vector>

// Separate-chaining hash map with string keys.
// Used to store category budgets and per-category spent totals in O(1).
template <typename V>
class HashMap {
private:
    struct Entry {
        std::string key;
        V value;
        Entry* next;
        Entry(const std::string& k, const V& v) : key(k), value(v), next(nullptr) {}
    };

    Entry** buckets;
    int     bucketCount;
    int     entryCount;

    unsigned long hashKey(const std::string& key) const {
        // Simple djb2 hash.
        unsigned long h = 5381;
        for (char c : key) h = ((h << 5) + h) + static_cast<unsigned char>(c);
        return h % bucketCount;
    }

public:
    explicit HashMap(int buckets_ = 31)
        : buckets(new Entry*[buckets_]()), bucketCount(buckets_), entryCount(0) {
        for (int i = 0; i < bucketCount; ++i) buckets[i] = nullptr;
    }

    ~HashMap() {
        clear();
        delete[] buckets;
    }

    HashMap(const HashMap&) = delete;
    HashMap& operator=(const HashMap&) = delete;

    // Insert or update.
    void set(const std::string& key, const V& value) {
        unsigned long idx = hashKey(key);
        Entry* cur = buckets[idx];
        while (cur) {
            if (cur->key == key) { cur->value = value; return; }
            cur = cur->next;
        }
        Entry* n = new Entry(key, value);
        n->next = buckets[idx];
        buckets[idx] = n;
        ++entryCount;
    }

    bool has(const std::string& key) const {
        unsigned long idx = hashKey(key);
        Entry* cur = buckets[idx];
        while (cur) {
            if (cur->key == key) return true;
            cur = cur->next;
        }
        return false;
    }

    // Returns pointer to value, or nullptr if missing.
    V* get(const std::string& key) {
        unsigned long idx = hashKey(key);
        Entry* cur = buckets[idx];
        while (cur) {
            if (cur->key == key) return &cur->value;
            cur = cur->next;
        }
        return nullptr;
    }

    const V* get(const std::string& key) const {
        unsigned long idx = hashKey(key);
        Entry* cur = buckets[idx];
        while (cur) {
            if (cur->key == key) return &cur->value;
            cur = cur->next;
        }
        return nullptr;
    }

    bool remove(const std::string& key) {
        unsigned long idx = hashKey(key);
        Entry* prev = nullptr;
        Entry* cur  = buckets[idx];
        while (cur) {
            if (cur->key == key) {
                if (prev) prev->next = cur->next;
                else      buckets[idx] = cur->next;
                delete cur;
                --entryCount;
                return true;
            }
            prev = cur;
            cur  = cur->next;
        }
        return false;
    }

    std::vector<std::string> keys() const {
        std::vector<std::string> out;
        for (int i = 0; i < bucketCount; ++i) {
            Entry* cur = buckets[i];
            while (cur) { out.push_back(cur->key); cur = cur->next; }
        }
        return out;
    }

    template <typename F>
    void forEach(F f) {
        for (int i = 0; i < bucketCount; ++i) {
            Entry* cur = buckets[i];
            while (cur) { f(cur->key, cur->value); cur = cur->next; }
        }
    }

    int  size()    const { return entryCount; }
    bool isEmpty() const { return entryCount == 0; }

    void clear() {
        for (int i = 0; i < bucketCount; ++i) {
            Entry* cur = buckets[i];
            while (cur) { Entry* n = cur->next; delete cur; cur = n; }
            buckets[i] = nullptr;
        }
        entryCount = 0;
    }
};

#endif
