#ifndef DATE_H
#define DATE_H

#include <string>

// Lightweight calendar date used as the BST key and as the bill due-date type.
// Dates are stored as YYYY-MM-DD strings internally and compared component-wise.
class Date {
public:
    int year;
    int month;
    int day;

    Date();
    Date(int y, int m, int d);

    static Date today();
    static Date parse(const std::string& yyyy_mm_dd);   // throws via cerr message + returns {0,0,0} on failure
    static bool tryParse(const std::string& yyyy_mm_dd, Date& out);

    std::string toString() const;             // "YYYY-MM-DD"
    int         daysFrom(const Date& other) const; // signed (this - other) in days

    bool operator<(const Date& o) const;
    bool operator==(const Date& o) const;
    bool operator<=(const Date& o) const;
    bool operator>(const Date& o)  const;
    bool operator>=(const Date& o) const;
};

#endif
