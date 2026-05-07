#include "Transaction.h"

#include <iostream>
#include <iomanip>

Transaction::Transaction()
    : transactionId(0), date(), amount(0.0), category(""), description("") {}

Transaction::Transaction(int id, const Date& d, double amt,
                         const std::string& cat, const std::string& desc)
    : transactionId(id), date(d), amount(amt), category(cat), description(desc) {}

Transaction::~Transaction() {}

int         Transaction::getTransactionId() const { return transactionId; }
Date        Transaction::getDate()          const { return date; }
double      Transaction::getAmount()        const { return amount; }
std::string Transaction::getCategory()      const { return category; }
std::string Transaction::getDescription()   const { return description; }

void Transaction::setDate(const Date& d)              { date = d; }
void Transaction::setAmount(double a)                  { amount = a; }
void Transaction::setCategory(const std::string& c)    { category = c; }
void Transaction::setDescription(const std::string& d) { description = d; }

void Transaction::displayTransaction() const {
    std::cout << "  [" << transactionId << "] "
              << date.toString() << "  "
              << std::fixed << std::setprecision(2) << "$" << amount
              << "  " << category;
    if (!description.empty()) std::cout << "  - " << description;
    std::cout << "\n";
}
