#include "Income.h"

#include <iostream>
#include <iomanip>

Income::Income() : Transaction(), source("Other") {}

Income::Income(int id, const Date& d, double amt,
               const std::string& cat, const std::string& desc,
               const std::string& src)
    : Transaction(id, d, amt, cat, desc), source(src) {}

std::string Income::getSource() const { return source; }
void        Income::setSource(const std::string& s) { source = s; }

void Income::displayTransaction() const {
    std::cout << "  [I" << transactionId << "] "
              << date.toString() << "  +$"
              << std::fixed << std::setprecision(2) << amount
              << "  " << category
              << "  from " << source;
    if (!description.empty()) std::cout << "  - " << description;
    std::cout << "\n";
}
