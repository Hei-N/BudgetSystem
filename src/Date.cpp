#include "Date.h"

#include <ctime>
#include <iostream>
#include <sstream>
#include <iomanip>

Date::Date() : year(1970), month(1), day(1) {}
Date::Date(int y, int m, int d) : year(y), month(m), day(d) {}

Date Date::today() {
    std::time_t t = std::time(nullptr);
    std::tm* tm = std::localtime(&t);
    return Date(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
}

bool Date::tryParse(const std::string& s, Date& out) {
    int y = 0, m = 0, d = 0;
    char dash1 = 0, dash2 = 0;
    std::istringstream iss(s);
    iss >> y >> dash1 >> m >> dash2 >> d;
    if (iss.fail() || dash1 != '-' || dash2 != '-') return false;
    if (m < 1 || m > 12 || d < 1 || d > 31) return false;
    out = Date(y, m, d);
    return true;
}

Date Date::parse(const std::string& s) {
    Date out;
    if (!tryParse(s, out)) {
        std::cerr << "Warning: invalid date '" << s << "', using today.\n";
        return Date::today();
    }
    return out;
}

std::string Date::toString() const {
    std::ostringstream oss;
    oss << std::setw(4) << std::setfill('0') << year  << "-"
        << std::setw(2) << std::setfill('0') << month << "-"
        << std::setw(2) << std::setfill('0') << day;
    return oss.str();
}

// Convert to days since 1970-01-01 using std::tm + mktime.
static long toEpochDays(const Date& d) {
    std::tm tm{};
    tm.tm_year = d.year - 1900;
    tm.tm_mon  = d.month - 1;
    tm.tm_mday = d.day;
    tm.tm_hour = 12; // avoid DST edges
    std::time_t t = std::mktime(&tm);
    if (t == (std::time_t)-1) return 0;
    return static_cast<long>(t / 86400);
}

int Date::daysFrom(const Date& other) const {
    long a = toEpochDays(*this);
    long b = toEpochDays(other);
    return static_cast<int>(a - b);
}

bool Date::operator<(const Date& o) const {
    if (year  != o.year)  return year  < o.year;
    if (month != o.month) return month < o.month;
    return day < o.day;
}
bool Date::operator==(const Date& o) const {
    return year == o.year && month == o.month && day == o.day;
}
bool Date::operator<=(const Date& o) const { return (*this < o) || (*this == o); }
bool Date::operator>(const Date& o)  const { return o < *this; }
bool Date::operator>=(const Date& o) const { return !(*this < o); }
